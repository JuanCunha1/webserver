#!/usr/bin/env python3
"""Reads a sibling file using a RELATIVE path. Only works if the server
runs the CGI with its cwd set to the script's own directory, as required
by the subject ("The CGI should be run in the correct directory for
relative path file access")."""
import sys

try:
    with open("reldata.txt", "r") as f:
        content = f.read().strip()
    status = "FOUND:" + content
except IOError as e:
    status = "NOTFOUND:%s" % e

body = status + "\n"
sys.stdout.write("Content-Type: text/plain\r\n")
sys.stdout.write("Content-Length: %d\r\n" % len(body))
sys.stdout.write("\r\n")
sys.stdout.write(body)
