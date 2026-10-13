#!/usr/bin/env python3
"""Crashes on purpose (uncaught exception -> non-zero exit, no CGI
headers at all). The server must turn this into a clean HTTP error
response instead of hanging or crashing itself."""
raise RuntimeError("intentional CGI failure for testing")
