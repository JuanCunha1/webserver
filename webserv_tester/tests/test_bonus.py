"""Bonus part (only meaningful if the mandatory part is fully solid, per
the evalsheet): cookies/session support, and multiple CGI interpreters.
These are soft/informational checks -- there's no bonus requirement in
the config format, so we just probe for the documented behaviour."""
import os
import shutil
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from lib import httpclient as hc
from lib import procman

HOST = "127.0.0.1"
PORT = 8520


def run(rec, ctx):
    rec.group("Bonus: cookies/session, multiple CGI types (informational)")

    proc = procman.WebservProcess(
        ctx["binary"], "configs/cgi.conf", ctx["tester_dir"],
        os.path.join(ctx["log_dir"], "bonus.log"), host=HOST, ports=(PORT,))
    if not proc.start():
        rec.crash("start server with configs/cgi.conf", proc.read_log_tail())
        return

    try:
        r = hc.request(HOST, PORT, "GET", "/env.py", headers={"Host": "localhost"})
        rec.info("Set-Cookie present on a plain response (no session feature exercised yet)",
                 "present" if r.header("set-cookie") else "absent")

        r2 = hc.request(HOST, PORT, "GET", "/env.py",
                         headers={"Host": "localhost", "Cookie": "sessionid=tester-probe-123"})
        cookie_echoed = "COOKIE" in r2.body.decode("utf-8", "replace") or bool(r2.header("set-cookie"))
        rec.info("server echoes/uses a client-supplied Cookie header in any visible way",
                 str(cookie_echoed))

        has_cookie_support = bool(r.header("set-cookie")) or bool(r2.header("set-cookie"))
        if has_cookie_support:
            rec.ok("cookies/session support (bonus): server issues Set-Cookie")
        else:
            rec.info("cookies/session support (bonus, subject Chapter VI)",
                     "no Set-Cookie observed on generic probes -- not a mandatory-part "
                     "requirement; ask the team to demo their session example directly "
                     "if they claim this bonus")

        php_cgi = shutil.which("php-cgi")
        rec.info("php-cgi available on this machine to test a second CGI type", bool(php_cgi))
        if php_cgi:
            rec.info("multiple CGI types (bonus)",
                     "php-cgi is installed but this tester ships no .php test route/config -- "
                     "ask the team to demo their second CGI interpreter directly")
        else:
            rec.skip("multiple CGI types (bonus)", "no second CGI interpreter (e.g. php-cgi) found on PATH")

        proc.assert_alive()
    except procman.CrashError:
        rec.crash("server crashed during bonus probing", proc.read_log_tail())
    finally:
        proc.stop()
