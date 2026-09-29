"""Config directives that PARSE successfully (and pass validation) but,
per static reading of the handler code, appear to never actually be
applied when building a response:

  - `return CODE URL;` (redirect)        -> src/core/Router.cpp is empty;
    nothing in ResponseBuilder ever reads loc.returnRedirections.
  - `error_page CODE path;`              -> ErrorHandler::handleError()
    always builds its own generic HTML; server.errorPages is never read
    outside the config parser.
  - `autoindex on;` on a directory with no index file -> GetHandler has
    the autoindex call commented out, so it always falls through to 403.
  - `upload_store path;`                 -> PostHandler always writes
    uploaded files under the resolved request path (locationRoot + URI),
    never under loc.uploadStore.

These are kept in their own module so a still-missing feature reads as
exactly that in the report, instead of being mixed in with core protocol
bugs. Each check states the subject requirement it corresponds to.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from lib import httpclient as hc
from lib import procman

HOST = "127.0.0.1"


def run(rec, ctx):
    rec.group("Configured-but-possibly-not-wired-up features (redirect / error_page / autoindex / upload_store)")

    proc = procman.WebservProcess(
        ctx["binary"], "configs/basic.conf", ctx["tester_dir"],
        os.path.join(ctx["log_dir"], "known_gaps.log"), host=HOST, ports=(8500, 8501))
    if not proc.start():
        rec.crash("start server with configs/basic.conf", proc.read_log_tail())
        return

    try:
        _test_redirect(rec)
        _test_error_page(rec)
        _test_autoindex(rec)
        _test_upload_store(rec)
        proc.assert_alive()
        rec.ok("server still alive after known-gaps probing")
    except procman.CrashError:
        rec.crash("server crashed while probing configured-but-unwired features", proc.read_log_tail())
    finally:
        proc.stop()


def _test_redirect(rec):
    r = hc.request(HOST, 8500, "GET", "/redirect/", headers={"Host": "localhost"}, timeout=3.0)
    is_redirect = r.status_code in (301, 302, 303, 307, 308) and r.header("location")
    rec.check(
        "location with `return 302 /index.html;` actually issues an HTTP redirect "
        "(subject IV.3: 'HTTP redirection' as a supported per-route config option)",
        is_redirect, "",
        "got %r instead of a 3xx + Location -- the parsed `return` directive "
        "(loc.returnRedirections) does not appear to be applied anywhere at request time"
        % (r,))


def _test_error_page(rec):
    r = hc.request(HOST, 8500, "GET", "/this-page-does-not-exist-xyz", headers={"Host": "localhost"})
    rec.check(
        "`error_page 404 www/errors/404.html;` custom page is served instead of the built-in one",
        b"marker:CUSTOM_404" in r.body,
        "",
        "got the built-in generic 404 body instead of the configured custom page -- "
        "server.errorPages is parsed/validated but ErrorHandler::handleError() always "
        "builds its own generic HTML (still fine per subject: 'must have default error "
        "pages IF NONE ARE PROVIDED' -- here one WAS provided in config)" if b"marker:CUSTOM_404" not in r.body else "")
    rec.check("even the built-in default 404 page is at least well-formed HTML with 404 in it",
              r.status_code == 404 and b"404" in r.body, "", repr(r))


def _test_autoindex(rec):
    r = hc.request(HOST, 8500, "GET", "/autoindex-on/", headers={"Host": "localhost"})
    looks_like_listing = r.status_code == 200 and (b"fileA.txt" in r.body or b"subdir" in r.body)
    rec.check(
        "`autoindex on;` on a directory with no index file actually generates a directory listing "
        "(subject IV.3: 'Enabling or disabling directory listing')",
        looks_like_listing, "",
        "got status=%s -- AutoIndex.cpp is present but empty and GetHandler's "
        "autoindex branch is commented out, so it falls through to 403 regardless "
        "of the autoindex setting" % r.status_code)

    r2 = hc.request(HOST, 8500, "GET", "/autoindex-off/", headers={"Host": "localhost"})
    rec.check("`autoindex off;` on the same kind of directory correctly returns 403 (not a listing)",
              r2.status_code == 403, "", repr(r2))


def _test_upload_store(rec):
    body = "content routed via upload_store?"
    r = hc.request(HOST, 8500, "POST", "/uploads/upload_store_probe.txt",
                    headers={"Host": "localhost", "Content-Type": "text/plain",
                             "Content-Length": str(len(body))}, body=body)
    rec.info("POST to a route with `upload_store` configured", repr(r))
    landed_where_expected = os.path.isfile(
        os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                     "www", "site_default", "uploads", "upload_store_probe.txt"))
    rec.check(
        "uploaded file lands in the directory the route's `root` resolves to "
        "(here it coincides with `upload_store`, so this alone doesn't prove upload_store "
        "is honored -- PostHandler always writes to locationRoot+URI and never reads "
        "loc.uploadStore at all)",
        landed_where_expected, "", "file not found where expected after POST")
    if landed_where_expected:
        hc.request(HOST, 8500, "DELETE", "/uploads/upload_store_probe.txt",
                   headers={"Host": "localhost"})
