"""Helpers to start/stop the webserv binary under test and watch for the
one thing the subject punishes with an automatic 0: crashing."""

import os
import signal
import subprocess
import time

from . import httpclient


class CrashError(Exception):
    def __init__(self, proc):
        self.proc = proc
        super(CrashError, self).__init__(
            "webserv exited unexpectedly (returncode=%r) while it should "
            "still be running -- see log at %s" % (proc.returncode, proc.log_path))


class WebservProcess(object):
    """Launch `binary config` with cwd=cwd, capture stdout/stderr to a log
    file, and offer crash-detection helpers.
    """

    def __init__(self, binary, config_path, cwd, log_path, host="127.0.0.1",
                 ports=(), env=None, extra_args=None):
        self.binary = binary
        self.config_path = config_path
        self.cwd = cwd
        self.log_path = log_path
        self.host = host
        self.ports = list(ports)
        self.env = env
        self.extra_args = extra_args or []
        self.proc = None
        self._log_fh = None
        self._stopped_deliberately = False

    def start(self, wait_ports=True, wait_timeout=5.0):
        self._log_fh = open(self.log_path, "wb")
        args = [self.binary, self.config_path] + self.extra_args
        self.proc = subprocess.Popen(
            args, cwd=self.cwd, stdout=self._log_fh, stderr=subprocess.STDOUT,
            env=self.env)
        self._stopped_deliberately = False

        # Give it a brief moment, then make sure it didn't die immediately
        # (e.g. bad config, port already in use, etc.)
        time.sleep(0.15)
        if self.proc.poll() is not None:
            return False

        if wait_ports:
            for p in self.ports:
                if not httpclient.wait_for_port(self.host, p, timeout=wait_timeout):
                    return False
        return True

    def is_alive(self):
        return self.proc is not None and self.proc.poll() is None

    def assert_alive(self):
        if self.proc is None:
            raise CrashError(self.proc)
        rc = self.proc.poll()
        if rc is not None:
            raise CrashError(self.proc)

    def returncode(self):
        return self.proc.poll() if self.proc else None

    def read_log_tail(self, n=4000):
        try:
            with open(self.log_path, "rb") as f:
                data = f.read()
            return data[-n:].decode("utf-8", "replace")
        except OSError:
            return ""

    def stop(self, timeout=3.0):
        if self.proc is None:
            return
        self._stopped_deliberately = True
        if self.proc.poll() is None:
            try:
                self.proc.send_signal(signal.SIGTERM)
            except OSError:
                pass
            deadline = time.time() + timeout
            while time.time() < deadline and self.proc.poll() is None:
                time.sleep(0.05)
            if self.proc.poll() is None:
                try:
                    self.proc.kill()
                except OSError:
                    pass
                self.proc.wait(timeout=2)
        if self._log_fh:
            self._log_fh.close()
            self._log_fh = None

    def rss_kb(self):
        """Best-effort RSS memory (KB) of the running process, via /proc."""
        if not self.is_alive():
            return None
        try:
            with open("/proc/%d/status" % self.proc.pid) as f:
                for line in f:
                    if line.startswith("VmRSS:"):
                        return int(line.split()[1])
        except (OSError, ValueError, IndexError):
            return None
        return None

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc, tb):
        self.stop()
