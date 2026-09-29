"""HTTP/1.x wire-level edge cases: malformed requests, chunked transfer
encoding, Host header rules, keep-alive/close semantics, partial/slow
clients. This is exactly the kind of thing the subject tells you to check
with `telnet`/`nc` before starting, and what the evalsheet's "Basic
checks" section exercises live with curl/telnet.

NOTE: this project has a confirmed bug (see test_basic_http.py) where any
request rejected by the parser with an HttpException (400/411/413/414/
501/505) is logged ("HTTP parse error: ...") but NO response is ever sent
and the connection is never closed -- the client just hangs. Every check
below that expects such a status code will therefore likely time out
too; we use a short per-check timeout (1.5-2s) to keep the whole suite's
runtime reasonable while still reliably distinguishing "hangs" from
"replies quickly".
"""
import os
import socket
import sys
import threading
import time

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from lib import httpclient as hc
from lib import procman

HOST = "127.0.0.1"
SHORT = 1.5


def run(rec, ctx):
    rec.group("HTTP/1.x protocol edge cases (headers, chunked, keep-alive, malformed input)")

    proc = procman.WebservProcess(
        ctx["binary"], "configs/basic.conf", ctx["tester_dir"],
        os.path.join(ctx["log_dir"], "protocol.log"), host=HOST, ports=(8500, 8501))
    if not proc.start():
        rec.crash("start server with configs/basic.conf", proc.read_log_tail())
        return

    try:
        _test_host_header(rec)
        _test_header_syntax(rec)
        _test_transfer_encoding(rec)
        _test_chunked(rec)
        _test_line_endings(rec)
        _test_connection_semantics(rec)
        _test_partial_and_slow_clients(rec, proc)
        proc.assert_alive()
        rec.ok("server still alive after protocol edge-case barrage")
    except procman.CrashError:
        rec.crash("server crashed during protocol edge-case tests", proc.read_log_tail())
    finally:
        proc.stop()


def _test_host_header(rec):
    scenario = "HTTP/1.1 request with NO Host header -> expected 400"
    r = hc.raw_request(HOST, 8500, "GET /index.html HTTP/1.1\r\n\r\n", timeout=SHORT)
    rec.check(scenario, r.status_code == 400, "", hc.describe_failure(r, scenario))

    scenario = "HTTP/1.1 request with EMPTY Host header -> expected 400"
    r = hc.raw_request(HOST, 8500, "GET /index.html HTTP/1.1\r\nHost:\r\n\r\n", timeout=SHORT)
    rec.check(scenario, r.status_code == 400, "", hc.describe_failure(r, scenario))

    scenario = "HTTP/1.1 request with TWO Host headers -> expected 400"
    r = hc.raw_request(HOST, 8500,
                        "GET /index.html HTTP/1.1\r\nHost: a\r\nHost: b\r\n\r\n", timeout=SHORT)
    rec.check(scenario, r.status_code == 400, "", hc.describe_failure(r, scenario))

    r = hc.raw_request(HOST, 8500, "GET /index.html HTTP/1.0\r\n\r\n", timeout=3.0)
    rec.check("HTTP/1.0 request with NO Host header -> allowed (200)",
              r.status_code == 200, "", hc.describe_failure(r, "HTTP/1.0 without Host"))


def _test_header_syntax(rec):
    scenario = "header line with no colon at all -> expected 400"
    r = hc.raw_request(HOST, 8500,
                        "GET /index.html HTTP/1.1\r\nHost: localhost\r\nThisIsNotAHeader\r\n\r\n",
                        timeout=SHORT)
    rec.check(scenario, r.status_code == 400, "", hc.describe_failure(r, scenario))

    scenario = "space before the colon in a header line -> expected 400"
    r = hc.raw_request(HOST, 8500,
                        "GET /index.html HTTP/1.1\r\nHost: localhost\r\nX-Foo : bar\r\n\r\n",
                        timeout=SHORT)
    rec.check(scenario, r.status_code == 400, "", hc.describe_failure(r, scenario))

    scenario = "control character in header name -> expected 400"
    r = hc.raw_request(HOST, 8500,
                        "GET /index.html HTTP/1.1\r\nHost: localhost\r\nX-\x01Foo: bar\r\n\r\n",
                        timeout=SHORT)
    rec.check(scenario, r.status_code == 400, "", hc.describe_failure(r, scenario))

    r = hc.raw_request(HOST, 8500,
                        "GET /index.html HTTP/1.1\r\nHost: localhost\r\n"
                        "X-Weird-But-Valid: value with spaces\r\n\r\n", timeout=3.0)
    rec.check("header value containing spaces is accepted (200)",
              r.status_code == 200, "", hc.describe_failure(r, "header value with spaces"))

    body = "hello"
    scenario = "identical duplicated Content-Length headers -> tolerated, expected 2xx"
    r = hc.raw_request(HOST, 8500,
                        "POST /uploads/dup_cl_ok.txt HTTP/1.1\r\nHost: localhost\r\n"
                        "Content-Length: 5\r\nContent-Length: 5\r\n\r\n" + body, timeout=3.0)
    rec.check(scenario, r.status_code is not None and 200 <= r.status_code < 300,
               "", hc.describe_failure(r, scenario))

    scenario = "duplicated Content-Length headers with DIFFERENT values -> expected 400"
    r = hc.raw_request(HOST, 8500,
                        "POST /uploads/dup_cl_bad.txt HTTP/1.1\r\nHost: localhost\r\n"
                        "Content-Length: 5\r\nContent-Length: 999\r\n\r\n" + body, timeout=SHORT)
    rec.check(scenario, r.status_code == 400, "", hc.describe_failure(r, scenario))

    hc.request(HOST, 8500, "DELETE", "/uploads/dup_cl_ok.txt", headers={"Host": "localhost"}, timeout=2.0)


def _test_transfer_encoding(rec):
    scenario = "Content-Length AND Transfer-Encoding both present -> expected 400"
    r = hc.raw_request(HOST, 8500,
                        "POST /uploads/both.txt HTTP/1.1\r\nHost: localhost\r\n"
                        "Content-Length: 5\r\nTransfer-Encoding: chunked\r\n\r\n5\r\nhello\r\n0\r\n\r\n",
                        timeout=SHORT)
    rec.check(scenario, r.status_code == 400, "", hc.describe_failure(r, scenario))

    scenario = "unsupported Transfer-Encoding value -> expected 501"
    r = hc.raw_request(HOST, 8500,
                        "POST /uploads/gzip.txt HTTP/1.1\r\nHost: localhost\r\n"
                        "Transfer-Encoding: gzip\r\n\r\n", timeout=SHORT)
    rec.check(scenario, r.status_code == 501, "", hc.describe_failure(r, scenario))


def _test_chunked(rec):
    body = b"4\r\nWiki\r\n5\r\npedia\r\n0\r\n\r\n"
    r = hc.raw_request(HOST, 8500,
                        b"POST /uploads/chunked_ok.txt HTTP/1.1\r\nHost: localhost\r\n"
                        b"Transfer-Encoding: chunked\r\n\r\n" + body, timeout=3.0)
    rec.check("well-formed chunked upload -> 201 Created", r.status_code == 201,
               "", hc.describe_failure(r, "chunked upload"))
    if r.status_code == 201:
        r2 = hc.request(HOST, 8500, "GET", "/uploads/chunked_ok.txt", headers={"Host": "localhost"})
        rec.check("un-chunked body was reassembled correctly ('Wikipedia')",
                  r2.body == b"Wikipedia", "", repr(r2))
        hc.request(HOST, 8500, "DELETE", "/uploads/chunked_ok.txt", headers={"Host": "localhost"})

    scenario = "chunk size that isn't valid hex -> expected 400"
    bad_body = b"ZZZ\r\nhello\r\n0\r\n\r\n"
    r = hc.raw_request(HOST, 8500,
                        b"POST /uploads/badchunk.txt HTTP/1.1\r\nHost: localhost\r\n"
                        b"Transfer-Encoding: chunked\r\n\r\n" + bad_body, timeout=SHORT)
    rec.check(scenario, r.status_code == 400, "", hc.describe_failure(r, scenario))

    scenario = "chunk data missing its trailing CRLF -> expected 400"
    bad_body2 = b"5\r\nhelloXX0\r\n\r\n"
    r = hc.raw_request(HOST, 8500,
                        b"POST /uploads/badchunk2.txt HTTP/1.1\r\nHost: localhost\r\n"
                        b"Transfer-Encoding: chunked\r\n\r\n" + bad_body2, timeout=SHORT)
    rec.check(scenario, r.status_code == 400, "", hc.describe_failure(r, scenario))


def _test_line_endings(rec):
    scenario = "bare LF (no CR) as line ending -> expected 400"
    r = hc.raw_request(HOST, 8500, "GET /index.html HTTP/1.1\nHost: localhost\n\n", timeout=SHORT)
    rec.check(scenario, r.status_code == 400, "", hc.describe_failure(r, scenario))


def _test_connection_semantics(rec):
    conn = hc.Connection(HOST, 8500, timeout=3.0)
    try:
        conn.connect()
        conn.send("GET /index.html HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n")
        r = conn.read_response()
        rec.check("explicit 'Connection: close' -> response says close",
                  (r.header("connection") or "").lower() == "close", "", repr(r.headers))
        time.sleep(0.3)
        try:
            extra = conn.sock.recv(16)
            rec.check("server actually closes the TCP connection after a 'Connection: close' response",
                      extra == b"", "", "still got bytes: %r" % extra)
        except socket.timeout:
            rec.fail("server actually closes the TCP connection after a 'Connection: close' response",
                      "recv() timed out instead of returning EOF -- the header says 'close' but the "
                      "socket is left open, so an HTTP/1.0-only or Connection:-close client would hang")
    finally:
        conn.close()

    conn = hc.Connection(HOST, 8500, timeout=3.0)
    try:
        conn.connect()
        sequence = ["/index.html", "/page.html", "/secretdir/hidden.txt", "/page.html"]
        expected_markers = [b"marker:INDEX_DEFAULT", b"marker:PAGE_STATIC",
                             b"marker:SECRETDIR_FILE", b"marker:PAGE_STATIC"]
        responses = []
        for uri in sequence:
            conn.send("GET %s HTTP/1.1\r\nHost: localhost\r\n\r\n" % uri)
            responses.append(conn.read_response())
        rec.check("keep-alive: first request on the connection succeeds",
                  responses[0].status_code == 200 and expected_markers[0] in responses[0].body,
                  "", repr(responses[0]))
        mismatches = [
            "request #%d for %s got body of request #1 instead (marker mismatch)" % (i + 1, sequence[i])
            for i in range(1, len(sequence))
            if expected_markers[i] not in responses[i].body
        ]
        rec.check(
            "CRITICAL: on a keep-alive (persistent) connection, EACH subsequent request "
            "returns the resource actually requested (not a repeat of request #1)",
            not mismatches,
            "",
            ("%d/%d follow-up requests on the SAME connection returned the WRONG resource: %s "
             "-- keep-alive connections are effectively broken after the first request, which "
             "will corrupt real multi-request browser sessions (subject: 'must be compatible "
             "with standard web browsers')" % (len(mismatches), len(sequence) - 1,
                                                "; ".join(mismatches))) if mismatches else "")
    except (socket.timeout, EOFError) as e:
        rec.fail("keep-alive: sequential requests on ONE connection all succeed", repr(e))
    finally:
        conn.close()

    conn = hc.Connection(HOST, 8500, timeout=3.0)
    try:
        conn.connect()
        conn.send(b"GET /index.html HTTP/1.1\r\nHost: localhost\r\n\r\n"
                   b"GET /page.html HTTP/1.1\r\nHost: localhost\r\n\r\n")
        r1 = conn.read_response()
        r2 = conn.read_response()
        rec.check("pipelined requests (2 requests in one write) both answered, in order",
                  r1.status_code == 200 and r2.status_code == 200
                  and b"marker:INDEX_DEFAULT" in r1.body and b"marker:PAGE_STATIC" in r2.body,
                  "", "r1=%r r2=%r" % (r1, r2))
    except (socket.timeout, EOFError) as e:
        rec.info("pipelined requests both answered, in order",
                 "not supported / timed out (not mandatory per subject): %r" % e)
    finally:
        conn.close()


def _test_partial_and_slow_clients(rec, proc):
    conn = hc.Connection(HOST, 8500, timeout=2.0)
    try:
        conn.connect()
        conn.send("GET /index.html HTTP/1.1\r\nHost: local")  # deliberately incomplete
    finally:
        conn.close()
    time.sleep(0.3)
    r = hc.request(HOST, 8500, "GET", "/index.html", headers={"Host": "localhost"})
    rec.check("server survives an abrupt disconnect mid-request (still serves new clients)",
              r.status_code == 200, "", hc.describe_failure(r, "post-disconnect sanity GET"))

    slow_result = {}

    def _slow_client():
        conn = hc.Connection(HOST, 8500, timeout=6.0)
        try:
            conn.connect()
            full = "GET /page.html HTTP/1.1\r\nHost: localhost\r\n\r\n"
            conn.send_slowly(full, chunk_size=1, delay=0.05)
            slow_result["resp"] = conn.read_response(max_time=5.0)
        except (socket.timeout, EOFError) as e:
            slow_result["error"] = e
        finally:
            conn.close()

    t = threading.Thread(target=_slow_client)
    t.start()
    time.sleep(0.2)  # let the slow client start trickling bytes in

    others_ok = True
    other_details = []
    for _ in range(5):
        r = hc.request(HOST, 8500, "GET", "/index.html", headers={"Host": "localhost"}, timeout=2.0)
        if r.status_code != 200:
            others_ok = False
            other_details.append(hc.describe_failure(r, "concurrent GET while a slow client is mid-request"))
    t.join(timeout=10.0)

    rec.check("OTHER clients keep getting served WHILE a slow client trickles its request in "
              "(proves the single poll() loop isn't blocked by one slow reader)",
              others_ok, "", "; ".join(other_details))

    resp = slow_result.get("resp")
    rec.check("the byte-at-a-time ('slowloris'-style) request itself eventually completes",
              resp is not None and resp.status_code == 200,
              "", repr(slow_result.get("error") or resp))
