#!/usr/bin/env python3
"""Dumps CGI meta-variables plus the request body it received on stdin.
Used to check that the server exposes the required CGI environment
(REQUEST_METHOD, QUERY_STRING, CONTENT_LENGTH, CONTENT_TYPE, ...) and that
the full client body/arguments reach the script, per subject IV.3."""
import os
import sys

body = sys.stdin.buffer.read()

keys = [
    "REQUEST_METHOD", "QUERY_STRING", "CONTENT_LENGTH", "CONTENT_TYPE",
    "SCRIPT_NAME", "SCRIPT_FILENAME", "SERVER_PROTOCOL", "GATEWAY_INTERFACE",
    "PATH_INFO", "SERVER_NAME", "SERVER_PORT", "REMOTE_ADDR",
]

lines = []
for k in keys:
    lines.append("%s=%s" % (k, os.environ.get(k, "<MISSING>")))
lines.append("CWD=%s" % os.getcwd())
lines.append("BODY_LEN=%d" % len(body))
lines.append("BODY=%s" % body.decode("utf-8", "replace"))

out = "\n".join(lines) + "\n"
sys.stdout.write("Content-Type: text/plain\r\n")
sys.stdout.write("Content-Length: %d\r\n" % len(out))
sys.stdout.write("\r\n")
sys.stdout.write(out)
