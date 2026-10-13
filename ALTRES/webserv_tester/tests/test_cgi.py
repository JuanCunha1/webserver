"""CGI: env vars, GET/POST, error handling, malformed CGI output, and the
subject's explicit "run in the correct directory" requirement -- the
evalsheet's "Check CGI" section."""
import os
import sys
import threading
import time

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from lib import httpclient as hc
from lib import procman

HOST = "127.0.0.1"
PORT = 8520


def run(rec, ctx):
    rec.group("CGI (env vars, GET/POST, error resilience)")

    proc = procman.WebservProcess(
        ctx["binary"], "configs/cgi.conf", ctx["tester_dir"],
        os.path.join(ctx["log_dir"], "cgi.log"), host=HOST, ports=(PORT,))
    if not proc.start():
        rec.crash("start server with configs/cgi.conf", proc.read_log_tail())
        return

    try:
        _test_env_and_get(rec)
        _test_post_body(rec)
        _test_cgi_errors(rec, proc)
        _test_cgi_status_header(rec)
        _test_cgi_concurrency(rec, proc)
        proc.assert_alive()
        rec.ok("server still alive after CGI tests")
    except procman.CrashError:
        rec.crash("server crashed during CGI tests", proc.read_log_tail())
    finally:
        proc.stop()


def _test_env_and_get(rec):
    r = hc.request(HOST, PORT, "GET", "/env.py?foo=bar&baz=1", headers={"Host": "localhost"})
    rec.check("GET on a .py route invokes the CGI (200)", r.status_code == 200, "", repr(r))
    text = r.body.decode("utf-8", "replace")
    rec.check("REQUEST_METHOD is exposed to the CGI", "REQUEST_METHOD=GET" in text, "", text[:400])
    rec.check("QUERY_STRING carries the client's query args to the CGI",
              "QUERY_STRING=foo=bar&baz=1" in text, "", text[:400])
    rec.check("SCRIPT_NAME/SCRIPT_FILENAME point at the requested script",
              "env.py" in text, "", text[:400])

    for missing in ("PATH_INFO", "SERVER_NAME", "SERVER_PORT", "REMOTE_ADDR"):
        present = ("%s=<MISSING>" % missing) not in text
        if present:
            rec.ok("optional CGI var %s is set" % missing)
        else:
            rec.info("optional CGI var %s is not set" % missing,
                     "not strictly required by the subject, but common in real CGI gateways")

    r = hc.request(HOST, PORT, "GET", "/reldata.py", headers={"Host": "localhost"})
    text = r.body.decode("utf-8", "replace")
    rec.check(
        "CGI is executed with its CWD set to the script's own directory "
        "(subject IV.3: 'CGI should be run in the correct directory for relative path file access')",
        text.strip().startswith("FOUND:"),
        "", "got %r -- relative-path sibling file lookup failed from inside the CGI" % text.strip())


def _test_post_body(rec):
    body = "name=tester&value=hello"
    r = hc.request(HOST, PORT, "POST", "/env.py", headers={
        "Host": "localhost", "Content-Type": "application/x-www-form-urlencoded",
        "Content-Length": str(len(body))}, body=body)
    rec.check("POST on a .py route invokes the CGI (200)", r.status_code == 200, "", repr(r))
    text = r.body.decode("utf-8", "replace")
    rec.check("POST body reaches the CGI on stdin, full and unmodified",
              ("BODY=%s" % body) in text, "", text[:400])
    rec.check("CONTENT_LENGTH is exposed to the CGI for POST",
              "CONTENT_LENGTH=%d" % len(body) in text, "", text[:400])
    rec.check(
        "CONTENT_TYPE is exposed to the CGI for POST "
        "(watch for header lookup being case-sensitive against lower-cased stored header names)",
        "CONTENT_TYPE=application/x-www-form-urlencoded" in text,
        "", "got: %s" % next((l for l in text.splitlines() if l.startswith("CONTENT_TYPE")), "<line missing>"))

    # Chunked POST into a CGI: server must un-chunk before handing the CGI its body.
    chunked_body = b"5\r\nhello\r\n0\r\n\r\n"
    r = hc.raw_request(HOST, PORT,
                        b"POST /env.py HTTP/1.1\r\nHost: localhost\r\n"
                        b"Transfer-Encoding: chunked\r\n\r\n" + chunked_body, timeout=4.0)
    rec.check("chunked POST body is un-chunked before being handed to the CGI",
              r.status_code == 200 and b"BODY=hello" in r.body and b"BODY_LEN=5" in r.body,
              "", repr(r))


def _test_cgi_errors(rec, proc):
    scenario = "CGI script that raises an uncaught exception -> clean HTTP error, not a hang/crash"
    r = hc.request(HOST, PORT, "GET", "/error.py", headers={"Host": "localhost"}, timeout=4.0)
    rec.check(scenario, r.status_code is not None and r.status_code >= 500,
               "", hc.describe_failure(r, scenario))

    scenario = "CGI script producing ZERO output -> clean HTTP error, not a hang/crash"
    r = hc.request(HOST, PORT, "GET", "/empty.py", headers={"Host": "localhost"}, timeout=4.0)
    rec.check(scenario, r.status_code is not None and r.status_code >= 400,
               "", hc.describe_failure(r, scenario))

    scenario = "CGI output with no header/body separator at all -> clean HTTP error (e.g. 502), not a hang"
    r = hc.request(HOST, PORT, "GET", "/noterm.py", headers={"Host": "localhost"}, timeout=4.0)
    rec.check(scenario, r.status_code is not None and r.status_code >= 400,
               "", hc.describe_failure(r, scenario))

    r = hc.request(HOST, PORT, "GET", "/nolength.py", headers={"Host": "localhost"}, timeout=5.0)
    rec.check("CGI output with NO Content-Length -> body captured via EOF, per subject IV.3",
              r.status_code == 200 and b"marker:NOLENGTH_BODY_" in r.body,
              "", hc.describe_failure(r, "CGI without Content-Length"))

    r = hc.request(HOST, PORT, "GET", "/bigoutput.py", headers={"Host": "localhost"}, timeout=8.0)
    rec.check("large (~1MB) CGI output is relayed completely, unmodified",
              r.status_code == 200 and len(r.body) > 900000,
              "", "status=%s len=%s" % (r.status_code, len(r.body)))

    proc.assert_alive()
    rec.ok("server survived every misbehaving-CGI scenario above without crashing")


def _test_cgi_status_header(rec):
    r = hc.request(HOST, PORT, "GET", "/statusredirect.py", headers={"Host": "localhost"})
    rec.check("CGI 'Status:' header is relayed as the actual HTTP status code",
              r.status_code == 302, "", repr(r))
    rec.check("CGI-provided Location header is relayed to the client",
              r.header("location") == "/index.html", "", repr(r.headers))


def _test_cgi_concurrency(rec, proc):
    """One CGI sleeping for 2s must not block a concurrent, unrelated static-ish
    request -- proves CGI I/O also goes through the single poll() loop instead
    of blocking the whole event loop."""
    slow_result = {}

    def _call_slow():
        slow_result["resp"] = hc.request(HOST, PORT, "GET", "/slow.py",
                                          headers={"Host": "localhost"}, timeout=6.0)

    t = threading.Thread(target=_call_slow)
    t0 = time.time()
    t.start()
    time.sleep(0.3)
    fast = hc.request(HOST, PORT, "GET", "/env.py", headers={"Host": "localhost"}, timeout=2.0)
    fast_elapsed = time.time() - t0
    t.join(timeout=8.0)

    rec.check("a fast request completes quickly WHILE a slow CGI (2s sleep) is still running",
              fast.status_code == 200 and fast_elapsed < 1.8,
              "", "fast request took %.2fs (status=%s) -- if this is >=2s, the slow CGI is "
                  "blocking the event loop" % (fast_elapsed, fast.status_code))
    slow = slow_result.get("resp")
    rec.check("the slow CGI itself still completes successfully",
              slow is not None and slow.status_code == 200 and b"marker:SLOW_DONE" in slow.body,
              "", repr(slow))
