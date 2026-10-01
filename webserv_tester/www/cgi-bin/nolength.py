#!/usr/bin/env python3
"""Valid CGI response but WITHOUT Content-Length. Per subject IV.3: "If no
content_length is returned from the CGI, EOF will mark the end of the
returned data." The gateway must read the CGI's stdout until EOF and use
that as the body (or forward it chunked), not truncate/hang."""
import sys
body = "marker:NOLENGTH_BODY_" + ("X" * 500) + "\n"
sys.stdout.write("Content-Type: text/plain\r\n")
sys.stdout.write("\r\n")
sys.stdout.write(body)
