"""client_max_body_size enforcement, per subject IV.3 and the evalsheet's
"Configuration" section ('Limit the size of the client request body')."""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from lib import httpclient as hc
from lib import procman

HOST = "127.0.0.1"
PORT = 8510
LIMIT = 100  # bytes, must match configs/bodylimit.conf's client_max_body_size


def run(rec, ctx):
    rec.group("client_max_body_size enforcement")

    proc = procman.WebservProcess(
        ctx["binary"], "configs/bodylimit.conf", ctx["tester_dir"],
        os.path.join(ctx["log_dir"], "bodylimit.log"), host=HOST, ports=(PORT,))
    if not proc.start():
        rec.crash("start server with configs/bodylimit.conf", proc.read_log_tail())
        return

    try:
        small_body = "x" * (LIMIT - 10)
        r = hc.request(HOST, PORT, "POST", "/under_limit.txt",
                        headers={"Host": "localhost", "Content-Type": "text/plain",
                                 "Content-Length": str(len(small_body))},
                        body=small_body, timeout=3.0)
        rec.check("POST body UNDER the configured limit succeeds",
                  r.status_code in (200, 201), "", repr(r))

        big_body = "x" * (LIMIT + 500)
        scenario = "POST body OVER the configured limit (Content-Length) -> expected 413"
        r = hc.request(HOST, PORT, "POST", "/over_limit.txt",
                        headers={"Host": "localhost", "Content-Type": "text/plain",
                                 "Content-Length": str(len(big_body))},
                        body=big_body, timeout=2.5)
        rec.check(scenario, r.status_code == 413, "", hc.describe_failure(r, scenario))

        # curl-shaped check, matching the evalsheet's suggested command almost verbatim.
        import subprocess
        curl_body = "BODY IS HERE " * 50  # comfortably over 100 bytes
        try:
            out = subprocess.run(
                ["curl", "-s", "-o", "/dev/null", "-w", "%{http_code}", "--max-time", "3",
                 "-X", "POST", "-H", "Content-Type: plain/text", "--data", curl_body,
                 "http://%s:%d/curl_over_limit.txt" % (HOST, PORT)],
                capture_output=True, text=True, timeout=5)
            code = out.stdout.strip()
            detail = ("curl got HTTP %r" % code if code != "000" else
                      "curl reported 000 (no response at all before --max-time) -- consistent with "
                      "the 413-hang bug found above: the server never answers an over-limit POST")
            rec.check("evalsheet-style `curl -X POST --data <over-limit body>` -> 413",
                      code == "413", "", detail)
        except (OSError, subprocess.SubprocessError) as e:
            rec.skip("evalsheet-style curl over-limit check", str(e))

        chunked_body = (b"32\r\n" + b"x" * 50 + b"\r\n"
                         b"96\r\n" + b"y" * 150 + b"\r\n"
                         b"0\r\n\r\n")
        scenario = "chunked POST whose total size exceeds the limit -> expected 413"
        r = hc.raw_request(HOST, PORT,
                            b"POST /over_limit_chunked.txt HTTP/1.1\r\nHost: localhost\r\n"
                            b"Transfer-Encoding: chunked\r\n\r\n" + chunked_body, timeout=2.5)
        rec.check(scenario, r.status_code == 413, "", hc.describe_failure(r, scenario))

        proc.assert_alive()
        rec.ok("server still alive after body-limit tests")
    except procman.CrashError:
        rec.crash("server crashed during body-limit tests", proc.read_log_tail())
    finally:
        proc.stop()
