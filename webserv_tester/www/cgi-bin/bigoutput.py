#!/usr/bin/env python3
"""Outputs a large body (bigger than typical pipe buffers) to exercise
partial, multi-read CGI-output handling."""
import sys
body = ("0123456789" * 100000) + "\n"  # ~1MB
sys.stdout.write("Content-Type: text/plain\r\n")
sys.stdout.write("Content-Length: %d\r\n" % len(body))
sys.stdout.write("\r\n")
sys.stdout.write(body)
