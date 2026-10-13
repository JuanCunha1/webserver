"""Security-oriented checks that go beyond the letter of the subject (which
doesn't mandate traversal protection) but matter for "must not crash" /
general correctness and are exactly the kind of thing evaluators poke at
under "Try anything you like" in the evalsheet's browser-check section.

Uses disposable canary files OUTSIDE any configured web root instead of
touching real system files (no /etc/passwd reads, no deleting anything
that matters), so this is safe to run against a real machine.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from lib import httpclient as hc
from lib import procman

HOST = "127.0.0.1"
TESTER_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def run(rec, ctx):
    rec.group("Security-adjacent robustness (path traversal, weird URIs)")

    canary_read = os.path.join(TESTER_DIR, "tmp", "canary_read.txt")
    with open(canary_read, "w") as f:
        f.write("CANARY_OUTSIDE_WEBROOT_CONTENT\n")

    canary_delete = os.path.join(TESTER_DIR, "tmp", "canary_delete.txt")
    with open(canary_delete, "w") as f:
        f.write("delete me if you can reach me\n")

    proc = procman.WebservProcess(
        ctx["binary"], "configs/basic.conf", ctx["tester_dir"],
        os.path.join(ctx["log_dir"], "security.log"), host=HOST, ports=(8500, 8501))
    if not proc.start():
        rec.crash("start server with configs/basic.conf", proc.read_log_tail())
        return

    try:
        # www/site_default/../../tmp/canary_read.txt  ==  webserv_tester/tmp/canary_read.txt
        traversal_uri = "/../../tmp/canary_read.txt"
        r = hc.raw_request(HOST, 8500,
                            "GET %s HTTP/1.1\r\nHost: localhost\r\n\r\n" % traversal_uri, timeout=3.0)
        leaked = r.status_code == 200 and b"CANARY_OUTSIDE_WEBROOT_CONTENT" in r.body
        rec.check(
            "CRITICAL: GET with '../' path segments cannot escape the configured document root",
            not leaked, "",
            ("path traversal via '..' successfully read a file OUTSIDE the web root "
             "(%s) -- ResponseBuilder builds `path = locationRoot + uri` with no "
             "normalization/containment check on '..' segments" % traversal_uri) if leaked else "")

        r = hc.raw_request(HOST, 8500,
                            "DELETE %s HTTP/1.1\r\nHost: localhost\r\n\r\n" % "/../../tmp/canary_delete.txt",
                            timeout=3.0)
        still_there = os.path.isfile(canary_delete)
        rec.check(
            "CRITICAL: DELETE with '../' path segments cannot delete files OUTSIDE the document root",
            still_there, "",
            ("DELETE via path traversal removed a file OUTSIDE the web root (status=%s) -- "
             "on a real deployment this could delete arbitrary files the server process "
             "has permission to remove" % r.status_code) if not still_there else "")

        r = hc.raw_request(HOST, 8500, "GET /index.html\x00.jpg HTTP/1.1\r\nHost: localhost\r\n\r\n",
                            timeout=2.5)
        rec.check("NUL byte in the URI does not crash the server / hang the connection",
                  r.error is None or not isinstance(r.error, __import__("socket").timeout),
                  "", hc.describe_failure(r, "NUL byte in URI"))
        proc.assert_alive()

        weird_cases = [
            ("//index.html", "//index.html"),
            ("/./index.html", "/./index.html"),
            ("/index.html/", "/index.html/"),
            ("/%2e%2e/index.html", "/%2e%2e/index.html"),
            ("very long query string (~5000 bytes, expected 414)", "/index.html?" + "a" * 5000),
        ]
        for label, weird in weird_cases:
            r = hc.request(HOST, 8500, "GET", weird, headers={"Host": "localhost"}, timeout=2.5)
            rec.check("weird-but-common URI form [%s] does not hang the server" % label,
                      r.status_code is not None,
                      "", hc.describe_failure(r, label))

        proc.assert_alive()
        rec.ok("server still alive after the security probes")
    except procman.CrashError:
        rec.crash("server crashed during security probes", proc.read_log_tail())
    finally:
        proc.stop()
        for p in (canary_read, canary_delete):
            try:
                os.remove(p)
            except OSError:
                pass
