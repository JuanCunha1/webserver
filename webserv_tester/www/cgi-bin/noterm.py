#!/usr/bin/env python3
"""Writes output with no header/body terminator at all (no blank line).
A conforming CGI gateway should treat this as a bad/incomplete CGI
response (classically 502 Bad Gateway) rather than hang or crash."""
import sys
sys.stdout.write("this is not a valid CGI response, no blank line anywhere")
