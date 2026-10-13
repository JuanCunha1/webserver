#!/usr/bin/env python3
"""Never terminates and never writes a byte. Used (opt-in, --slow) to
check that the server does not hang forever / does not wedge its single
poll() loop when a CGI child misbehaves."""
import time
while True:
    time.sleep(1)
