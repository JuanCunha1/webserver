"""Core GET/POST/DELETE behaviour, status-code accuracy, per-route allowed
methods and multi-port serving -- the evalsheet's "Basic checks" and
"Configuration" sections."""
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from lib import httpclient as hc
from lib import procman

HOST = "127.0.0.1"


def run(rec, ctx):
    rec.group("Basic HTTP: GET / POST / DELETE, status codes, allowed methods")

    proc = procman.WebservProcess(
        ctx["binary"], "configs/basic.conf", ctx["tester_dir"],
        os.path.join(ctx["log_dir"], "basic.log"), host=HOST, ports=(8500, 8501))
    if not proc.start():
        rec.crash("start server with configs/basic.conf", proc.read_log_tail())
        return

    try:
        _test_get(rec, proc)
        _test_unknown_and_case(rec, proc)
        _test_allowed_methods(rec, proc)
        _test_post_lifecycle(rec, proc)
        _test_delete_edgecases(rec, proc)
        _test_multiport(rec, proc)
        proc.assert_alive()
        rec.ok("server still alive after this whole test group")
    except procman.CrashError as e:
        rec.crash("server crashed during Basic HTTP tests", proc.read_log_tail())
    finally:
        proc.stop()


def _test_get(rec, proc):
    r = hc.request(HOST, 8500, "GET", "/", headers={"Host": "localhost"})
    rec.check("GET / -> 200", r.status_code == 200, "", repr(r))
    rec.check("GET / body contains index marker", b"marker:INDEX_DEFAULT" in r.body, "", r.body[:200])
    rec.check("GET / has Content-Length header", r.header("content-length") is not None)
    if r.header("content-length") is not None:
        rec.check("Content-Length matches actual body size",
                  int(r.header("content-length")) == len(r.body),
                  "", "header=%s actual=%d" % (r.header("content-length"), len(r.body)))
    rec.check("GET / has Content-Type header", r.header("content-type") is not None)

    r = hc.request(HOST, 8500, "GET", "/page.html", headers={"Host": "localhost"})
    rec.check("GET /page.html -> 200 with correct body", r.status_code == 200 and b"marker:PAGE_STATIC" in r.body,
               "", repr(r))

    r = hc.request(HOST, 8500, "GET", "/secretdir/hidden.txt", headers={"Host": "localhost"})
    rec.check("GET nested static file -> 200", r.status_code == 200 and b"marker:SECRETDIR_FILE" in r.body,
               "", repr(r))

    r = hc.request(HOST, 8500, "GET", "/does-not-exist-at-all", headers={"Host": "localhost"})
    rec.check("GET missing file -> 404", r.status_code == 404, "", repr(r))
    rec.check("404 response reason phrase is 'Not Found'", r.reason == "Not Found", "", r.reason)

    r = hc.request(HOST, 8500, "GET", "/secretdir", headers={"Host": "localhost"})
    rec.check("GET a directory with no index file -> 403 (no autoindex configured here)",
              r.status_code == 403, "", repr(r))


def _test_unknown_and_case(rec, proc):
    scenario = "PUT (unsupported but syntactically valid method) -> expected 501"
    r = hc.request(HOST, 8500, "PUT", "/index.html", headers={"Host": "localhost"}, timeout=3.0)
    rec.check(scenario, r.status_code == 501, "", hc.describe_failure(r, scenario))

    scenario = "PATCH -> expected 501"
    r = hc.request(HOST, 8500, "PATCH", "/index.html", headers={"Host": "localhost"}, timeout=3.0)
    rec.check(scenario, r.status_code == 501, "", hc.describe_failure(r, scenario))

    scenario = "lowercase method 'get' -> expected 400 Bad Request"
    r = hc.raw_request(HOST, 8500, "get /index.html HTTP/1.1\r\nHost: localhost\r\n\r\n", timeout=3.0)
    rec.check(scenario, r.status_code == 400, "", hc.describe_failure(r, scenario))

    scenario = "bogus HTTP version 'HTTP/9.9' -> expected 505"
    r = hc.raw_request(HOST, 8500, "GET /index.html HTTP/9.9\r\nHost: localhost\r\n\r\n", timeout=3.0)
    rec.check(scenario, r.status_code == 505, "", hc.describe_failure(r, scenario))

    scenario = "extra token on the request line -> expected 400"
    r = hc.raw_request(HOST, 8500, "GET /index.html HTTP/1.1 extra-garbage\r\nHost: localhost\r\n\r\n", timeout=3.0)
    rec.check(scenario, r.status_code == 400, "", hc.describe_failure(r, scenario))

    scenario = "URI not starting with '/' -> expected 400"
    r = hc.raw_request(HOST, 8500, "GET index.html HTTP/1.1\r\nHost: localhost\r\n\r\n", timeout=3.0)
    rec.check(scenario, r.status_code == 400, "", hc.describe_failure(r, scenario))

    long_uri = "/" + ("a" * 3000)
    scenario = "URI longer than 2048 bytes -> expected 414"
    r = hc.raw_request(HOST, 8500, "GET %s HTTP/1.1\r\nHost: localhost\r\n\r\n" % long_uri, timeout=3.0)
    rec.check(scenario, r.status_code == 414, "", hc.describe_failure(r, scenario))


def _test_allowed_methods(rec, proc):
    r = hc.request(HOST, 8500, "GET", "/readonly/page.html", headers={"Host": "localhost"})
    rec.check("GET on GET-only route -> 200", r.status_code == 200, "", repr(r))

    r = hc.request(HOST, 8500, "POST", "/readonly/page.html",
                    headers={"Host": "localhost", "Content-Length": "0"})
    rec.check("POST on GET-only route -> 405 Method Not Allowed", r.status_code == 405, "", repr(r))

    r = hc.request(HOST, 8500, "DELETE", "/readonly/page.html", headers={"Host": "localhost"})
    rec.check("DELETE on GET-only route -> 405 Method Not Allowed", r.status_code == 405, "", repr(r))

    r = hc.request(HOST, 8500, "GET", "/", headers={"Host": "localhost"})
    r2 = hc.request(HOST, 8500, "DELETE", "/", headers={"Host": "localhost"})
    rec.info("DELETE / (root dir, allow_methods includes DELETE) response",
             repr(r2))


def _test_post_lifecycle(rec, proc):
    path = "/uploads/lifecycle_test.txt"
    body = "hello from the tester\n" * 5

    r = hc.request(HOST, 8500, "DELETE", path, headers={"Host": "localhost"})
    # cleanup from a previous run; ignore result

    r = hc.request(HOST, 8500, "POST", path,
                    headers={"Host": "localhost", "Content-Type": "text/plain",
                             "Content-Length": str(len(body))},
                    body=body)
    rec.check("POST new file -> 201 Created", r.status_code == 201, "", repr(r))
    rec.check("201 response has Location header", r.header("location") is not None)

    r = hc.request(HOST, 8500, "GET", path, headers={"Host": "localhost"})
    rec.check("GET the just-uploaded file -> 200 with matching body",
              r.status_code == 200 and r.body.decode("utf-8", "replace") == body,
              "", repr(r))

    r = hc.request(HOST, 8500, "POST", path,
                    headers={"Host": "localhost", "Content-Type": "text/plain",
                             "Content-Length": "5"},
                    body="hello")
    rec.check("POST to an EXISTING file -> 200 OK (not 201)", r.status_code == 200, "", repr(r))

    r = hc.request(HOST, 8500, "DELETE", path, headers={"Host": "localhost"})
    rec.check("DELETE existing file -> 204 No Content", r.status_code == 204, "", repr(r))

    r = hc.request(HOST, 8500, "GET", path, headers={"Host": "localhost"})
    rec.check("GET after DELETE -> 404", r.status_code == 404, "", repr(r))

    r = hc.request(HOST, 8500, "DELETE", path, headers={"Host": "localhost"})
    rec.check("DELETE an already-missing file -> 404 (not 500/403)", r.status_code == 404, "", repr(r))

    # multipart/form-data upload
    boundary = "----testerBoundary123"
    filename = "multipart_test.bin"
    filedata = os.urandom(64)
    parts = []
    parts.append(("--%s\r\n" % boundary).encode())
    parts.append(('Content-Disposition: form-data; name="file"; filename="%s"\r\n'
                  'Content-Type: application/octet-stream\r\n\r\n' % filename).encode())
    parts.append(filedata)
    parts.append(("\r\n--%s--\r\n" % boundary).encode())
    mp_body = b"".join(parts)

    r = hc.request(HOST, 8500, "POST", "/uploads/",
                    headers={"Host": "localhost",
                             "Content-Type": "multipart/form-data; boundary=%s" % boundary,
                             "Content-Length": str(len(mp_body))},
                    body=mp_body)
    rec.check("multipart/form-data upload -> 201 Created", r.status_code == 201, "", repr(r))

    r = hc.request(HOST, 8500, "GET", "/uploads/%s" % filename, headers={"Host": "localhost"})
    rec.check("GET the multipart-uploaded file back -> 200 with identical bytes",
              r.status_code == 200 and r.body == filedata,
              "", "status=%s len(got)=%d len(want)=%d" % (r.status_code, len(r.body), len(filedata)))
    hc.request(HOST, 8500, "DELETE", "/uploads/%s" % filename, headers={"Host": "localhost"})

    # application/x-www-form-urlencoded
    form_body = "field1=hello+world&field2=%40test"
    r = hc.request(HOST, 8500, "POST", "/",
                    headers={"Host": "localhost",
                             "Content-Type": "application/x-www-form-urlencoded",
                             "Content-Length": str(len(form_body))},
                    body=form_body)
    rec.check("application/x-www-form-urlencoded POST -> 2xx, no crash",
              r.status_code is not None and 200 <= r.status_code < 300, "", repr(r))

    # POST without Content-Length or Transfer-Encoding
    scenario = "POST with no Content-Length/Transfer-Encoding -> expected 411 Length Required"
    r = hc.raw_request(HOST, 8500,
                        "POST /uploads/should_fail.txt HTTP/1.1\r\nHost: localhost\r\n\r\n",
                        timeout=3.0)
    rec.check(scenario, r.status_code == 411, "", hc.describe_failure(r, scenario))


def _test_delete_edgecases(rec, proc):
    r = hc.request(HOST, 8500, "DELETE", "/secretdir", headers={"Host": "localhost"})
    rec.check("DELETE a directory -> 403 Forbidden (not removed, no crash)",
              r.status_code == 403, "", repr(r))
    r = hc.request(HOST, 8500, "GET", "/secretdir/hidden.txt", headers={"Host": "localhost"})
    rec.check("directory survives the attempted DELETE", r.status_code == 200, "", repr(r))


def _test_multiport(rec, proc):
    r1 = hc.request(HOST, 8500, "GET", "/", headers={"Host": "localhost"})
    r2 = hc.request(HOST, 8501, "GET", "/", headers={"Host": "localhost"})
    rec.check("port 8500 and 8501 serve DIFFERENT content (distinct websites)",
              r1.status_code == 200 and r2.status_code == 200 and r1.body != r2.body,
              "", "8500 body=%r 8501 body=%r" % (r1.body[:80], r2.body[:80]))
    rec.check("secondary site (8501) has its own marker",
              b"marker:INDEX_SITE_B" in r2.body, "", repr(r2))
