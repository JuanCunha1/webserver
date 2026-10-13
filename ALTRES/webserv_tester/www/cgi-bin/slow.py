#!/usr/bin/env python3
"""Sleeps for a couple of seconds before answering, used to prove the
server keeps serving OTHER clients concurrently (single poll() loop)
while this one CGI is still running."""
import sys
import time
time.sleep(2)
body = "marker:SLOW_DONE\n"
sys.stdout.write("Content-Type: text/plain\r\n")
sys.stdout.write("Content-Length: %d\r\n" % len(body))
sys.stdout.write("\r\n")
sys.stdout.write(body)
