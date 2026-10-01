"""Concurrency / availability / memory-leak checks, mirroring the
evalsheet's "Siege & stress test" section. Uses `siege` if it's on PATH
(the evalsheet literally tells you to `apt-get install siege`); otherwise
falls back to a pure-Python concurrent load generator so the check still
runs without extra setup.

Only runs the heavier variant when ctx["stress"] is truthy (the `--stress`
CLI flag) -- the light variant always runs so a default invocation still
gets *some* concurrency signal.
"""
import os
import shutil
import subprocess
import sys
import threading
import time

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from lib import httpclient as hc
from lib import procman

HOST = "127.0.0.1"
PORT = 8560


def run(rec, ctx):
    rec.group("Concurrency, availability and memory stability under load")

    proc = procman.WebservProcess(
        ctx["binary"], "configs/stress.conf", ctx["tester_dir"],
        os.path.join(ctx["log_dir"], "stress.log"), host=HOST, ports=(PORT,))
    if not proc.start():
        rec.crash("start server with configs/stress.conf", proc.read_log_tail())
        return

    try:
        duration = 20.0 if ctx.get("stress") else 5.0
        concurrency = 20 if ctx.get("stress") else 8
        _custom_load(rec, proc, duration, concurrency)
        _siege_if_available(rec, ctx, duration=10.0 if ctx.get("stress") else 4.0)
        proc.assert_alive()
        rec.ok("server still alive and responsive after the stress run")
    except procman.CrashError:
        rec.crash("server crashed under load", proc.read_log_tail())
    finally:
        proc.stop()


def _custom_load(rec, proc, duration, concurrency):
    stop_at = time.time() + duration
    counters = {"ok": 0, "fail": 0}
    lock = threading.Lock()
    rss_samples = []

    def worker():
        while time.time() < stop_at:
            r = hc.request(HOST, PORT, "GET", "/", headers={"Host": "localhost"}, timeout=3.0)
            with lock:
                if r.status_code == 200:
                    counters["ok"] += 1
                else:
                    counters["fail"] += 1

    def sampler():
        while time.time() < stop_at:
            v = proc.rss_kb()
            if v is not None:
                rss_samples.append((time.time(), v))
            time.sleep(0.5)

    threads = [threading.Thread(target=worker) for _ in range(concurrency)]
    st = threading.Thread(target=sampler)
    t0 = time.time()
    st.start()
    for t in threads:
        t.start()
    for t in threads:
        t.join()
    st.join()
    elapsed = time.time() - t0

    total = counters["ok"] + counters["fail"]
    availability = (100.0 * counters["ok"] / total) if total else 0.0
    rec.info("custom load: %d concurrent workers for %.0fs" % (concurrency, elapsed),
             "%d requests, %d ok, %d failed -> %.2f req/s" % (total, counters["ok"], counters["fail"],
                                                               total / elapsed if elapsed else 0))
    rec.check("availability under concurrent GET load is >= 99.5%% (evalsheet's own siege -b threshold)",
              availability >= 99.5, "", "measured %.2f%% over %d requests" % (availability, total))

    if len(rss_samples) >= 4:
        first_half = [v for _, v in rss_samples[:len(rss_samples) // 2]]
        second_half = [v for _, v in rss_samples[len(rss_samples) // 2:]]
        avg1 = sum(first_half) / len(first_half)
        avg2 = sum(second_half) / len(second_half)
        growth = avg2 - avg1
        rec.info("RSS memory: first-half avg %.0f KB -> second-half avg %.0f KB" % (avg1, avg2), "")
        rec.check(
            "RSS memory does not grow unboundedly during the load run (crude leak signal; "
            "confirm with valgrind/leaks for a real verdict)",
            growth < max(20 * 1024, avg1 * 0.5),
            "", "grew by %.0f KB across the run -- re-check with valgrind if this is large" % growth)
    else:
        rec.skip("RSS memory growth check", "could not sample /proc/<pid>/status enough times")

    r = hc.request(HOST, PORT, "GET", "/", headers={"Host": "localhost"}, timeout=3.0)
    rec.check("server responds promptly right after the load burst (no pile-up / hanging connections)",
              r.status_code == 200, "", repr(r))


def _siege_if_available(rec, ctx, duration):
    siege = shutil.which("siege")
    if not siege:
        rec.skip("siege stress test", "siege is not installed (evalsheet: `sudo apt-get install siege`)")
        return
    url = "http://%s:%d/" % (HOST, PORT)
    try:
        p = subprocess.run(
            [siege, "-b", "-c", "10", "-t", "%ds" % int(duration), url],
            capture_output=True, text=True, timeout=duration + 15)
        out = p.stdout + p.stderr
        rec.info("siege -b -c 10 -t %ds %s" % (int(duration), url), out[-800:])
        avail_line = next((l for l in out.splitlines() if "Availability" in l), None)
        if avail_line:
            try:
                pct = float(avail_line.split(":")[1].strip().rstrip("%"))
                rec.check("siege-reported availability >= 99.5%%", pct >= 99.5, "", avail_line)
            except (ValueError, IndexError):
                rec.info("could not parse siege availability line", avail_line)
    except (OSError, subprocess.SubprocessError) as e:
        rec.skip("siege stress test", str(e))
