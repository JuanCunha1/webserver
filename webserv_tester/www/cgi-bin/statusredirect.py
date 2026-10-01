#!/usr/bin/env python3
"""CGI emitting a non-200 Status header, per the CGI spec's own redirect
convention. The gateway is expected to relay it as the HTTP status."""
import sys
body = "moved\n"
sys.stdout.write("Status: 302 Found\r\n")
sys.stdout.write("Location: /index.html\r\n")
sys.stdout.write("Content-Type: text/plain\r\n")
sys.stdout.write("Content-Length: %d\r\n" % len(body))
sys.stdout.write("\r\n")
sys.stdout.write(body)
