"""Minimal raw HTTP/1.x client built on top of plain sockets.

Deliberately does NOT use `http.client` / `requests` for the request side,
so we can send malformed / edge-case bytes on the wire (bad request lines,
duplicate headers, LF-only line endings, chunked bodies with weird sizes,
pipelined requests, partial writes, ...). This mirrors what an evaluator
does with `telnet` / `nc`, per the subject's recommendation.
"""

import socket
import time


class Response(object):
    def __init__(self, status_code=None, reason="", headers=None, body=b"",
                 raw=b"", error=None):
        self.status_code = status_code
        self.reason = reason
        self.headers = headers or {}
        self.body = body
        self.raw = raw
        self.error = error

    @property
    def ok(self):
        return self.error is None and self.status_code is not None

    def header(self, name):
        return self.headers.get(name.lower())

    def __repr__(self):
        if self.error:
            return "<Response error=%r>" % (self.error,)
        return "<Response %s %s, %d headers, %d body bytes>" % (
            self.status_code, self.reason, len(self.headers), len(self.body))


def _split_status_line(line):
    parts = line.split(" ", 2)
    if len(parts) < 2:
        raise ValueError("bad status line: %r" % line)
    version = parts[0]
    try:
        code = int(parts[1])
    except ValueError:
        raise ValueError("bad status code in status line: %r" % line)
    reason = parts[2] if len(parts) > 2 else ""
    return version, code, reason


class Connection(object):
    """A raw TCP connection to the server, reusable for keep-alive tests."""

    def __init__(self, host, port, timeout=5.0):
        self.host = host
        self.port = port
        self.timeout = timeout
        self.sock = None
        self._buf = b""

    def connect(self):
        self.sock = socket.create_connection((self.host, self.port), timeout=self.timeout)
        self.sock.settimeout(self.timeout)
        return self

    def __enter__(self):
        return self.connect()

    def __exit__(self, exc_type, exc, tb):
        self.close()

    def close(self):
        if self.sock is not None:
            try:
                self.sock.close()
            except OSError:
                pass
            self.sock = None

    def send(self, data):
        if isinstance(data, str):
            data = data.encode("latin-1")
        self.sock.sendall(data)

    def send_slowly(self, data, chunk_size=1, delay=0.05):
        """Send data one (or a few) byte(s) at a time, to exercise the
        server's partial-read / non-blocking handling (slowloris-style)."""
        if isinstance(data, str):
            data = data.encode("latin-1")
        for i in range(0, len(data), chunk_size):
            self.sock.sendall(data[i:i + chunk_size])
            time.sleep(delay)

    def _recv_more(self):
        chunk = self.sock.recv(65536)
        if chunk == b"":
            raise EOFError("connection closed by peer")
        self._buf += chunk
        return chunk

    def read_response(self, read_body=True, max_time=None):
        """Read exactly one HTTP response from the stream, leaving any
        extra bytes (e.g. a pipelined second response) buffered for the
        next call."""
        deadline = time.time() + (max_time if max_time is not None else self.timeout)

        while b"\r\n\r\n" not in self._buf and b"\n\n" not in self._buf:
            if time.time() > deadline:
                raise socket.timeout("timed out waiting for headers")
            self._recv_more()

        if b"\r\n\r\n" in self._buf:
            head, rest = self._buf.split(b"\r\n\r\n", 1)
            sep_len = 4
        else:
            head, rest = self._buf.split(b"\n\n", 1)
            sep_len = 2
        self._buf = rest

        lines = head.replace(b"\r\n", b"\n").split(b"\n")
        status_line = lines[0].decode("latin-1")
        version, code, reason = _split_status_line(status_line)

        headers = {}
        header_order = []
        for line in lines[1:]:
            if not line:
                continue
            line = line.decode("latin-1")
            if ":" not in line:
                continue
            k, v = line.split(":", 1)
            k = k.strip().lower()
            v = v.strip()
            headers[k] = v
            header_order.append(k)

        body = b""
        if read_body and code != 204 and code != 304:
            content_length = headers.get("content-length")
            transfer_encoding = headers.get("transfer-encoding", "")

            if transfer_encoding.lower() == "chunked":
                body = self._read_chunked_body(deadline)
            elif content_length is not None:
                needed = int(content_length)
                while len(self._buf) < needed:
                    if time.time() > deadline:
                        raise socket.timeout("timed out waiting for body")
                    self._recv_more()
                body = self._buf[:needed]
                self._buf = self._buf[needed:]
            elif headers.get("connection", "").lower() == "close":
                try:
                    while True:
                        if time.time() > deadline:
                            break
                        self._recv_more()
                except EOFError:
                    pass
                body = self._buf
                self._buf = b""
            # else: no length info and not "close" -> assume no body (e.g. keep-alive
            # responses to HEAD-like semantics); leave body empty.

        resp = Response(status_code=code, reason=reason, headers=headers, body=body,
                         raw=head + b"\r\n\r\n" + body)
        resp.version = version
        resp.header_order = header_order
        return resp

    def _read_chunked_body(self, deadline):
        body = b""
        while True:
            while b"\r\n" not in self._buf:
                if time.time() > deadline:
                    raise socket.timeout("timed out reading chunk size")
                self._recv_more()
            size_line, self._buf = self._buf.split(b"\r\n", 1)
            size_str = size_line.split(b";", 1)[0].strip()
            size = int(size_str, 16)
            if size == 0:
                while b"\r\n\r\n" not in (self._buf + b"\r\n") and b"\r\n" != self._buf[:2]:
                    if self._buf.startswith(b"\r\n"):
                        break
                    if time.time() > deadline:
                        raise socket.timeout("timed out reading trailer")
                    if b"\r\n" in self._buf:
                        break
                    self._recv_more()
                if self._buf.startswith(b"\r\n"):
                    self._buf = self._buf[2:]
                break
            while len(self._buf) < size + 2:
                if time.time() > deadline:
                    raise socket.timeout("timed out reading chunk data")
                self._recv_more()
            body += self._buf[:size]
            self._buf = self._buf[size + 2:]
        return body


def request(host, port, method, path, headers=None, body=b"", version="HTTP/1.1",
            timeout=4.0, read_body=True, extra_raw=None):
    """One-shot convenience request: connect, send, read one response, close."""
    headers = dict(headers or {})
    if isinstance(body, str):
        body = body.encode("utf-8")

    lines = ["%s %s %s" % (method, path, version)]
    for k, v in headers.items():
        lines.append("%s: %s" % (k, v))
    head = "\r\n".join(lines) + "\r\n\r\n"
    data = head.encode("latin-1") + body
    if extra_raw:
        data += extra_raw

    conn = Connection(host, port, timeout=timeout)
    try:
        conn.connect()
        conn.send(data)
        resp = conn.read_response(read_body=read_body, max_time=timeout)
        return resp
    except (socket.timeout, socket.error, EOFError, ValueError) as e:
        return Response(error=e)
    finally:
        conn.close()


def raw_request(host, port, raw_bytes, timeout=4.0, read_body=True):
    """Send exactly the given raw bytes (already a full request) and read
    one response back."""
    if isinstance(raw_bytes, str):
        raw_bytes = raw_bytes.encode("latin-1")
    conn = Connection(host, port, timeout=timeout)
    try:
        conn.connect()
        conn.send(raw_bytes)
        return conn.read_response(read_body=read_body, max_time=timeout)
    except (socket.timeout, socket.error, EOFError, ValueError) as e:
        return Response(error=e)
    finally:
        conn.close()


def wait_for_port(host, port, timeout=5.0):
    deadline = time.time() + timeout
    while time.time() < deadline:
        try:
            with socket.create_connection((host, port), timeout=0.3):
                return True
        except OSError:
            time.sleep(0.05)
    return False


def describe_failure(resp, scenario):
    """Human-friendly failure message, calling out server hangs explicitly
    (the subject: "A request to your server should never hang indefinitely")."""
    if resp.error is not None:
        if isinstance(resp.error, socket.timeout):
            return ("SERVER HUNG: no response and connection not closed for: %s "
                    "-- violates 'a request should never hang indefinitely'" % scenario)
        return "connection error for %s: %r" % (scenario, resp.error)
    return "got %s %s for %s" % (resp.status_code, resp.reason, scenario)


def port_is_free(host, port):
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(0.3)
    try:
        s.connect((host, port))
        s.close()
        return False
    except OSError:
        s.close()
        return True
