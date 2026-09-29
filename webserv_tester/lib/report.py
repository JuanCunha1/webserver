import sys
import time

RESET = "\033[0m"
COLORS = {
    "PASS": "\033[32m",
    "FAIL": "\033[31m",
    "SKIP": "\033[33m",
    "CRASH": "\033[41;97m",
    "INFO": "\033[36m",
}


def _c(status, text):
    if not sys.stdout.isatty():
        return text
    return COLORS.get(status, "") + text + RESET


class Recorder(object):
    def __init__(self, quiet=False):
        self.results = []  # list of dict(group, name, status, msg)
        self.quiet = quiet
        self._group = None

    def group(self, name):
        self._group = name
        print("\n== %s ==" % name)

    MAX_LEN = 300

    def _trunc(self, s):
        if s is None:
            return s
        if not isinstance(s, str):
            s = repr(s)
        if len(s) > self.MAX_LEN:
            return s[:self.MAX_LEN] + "... [truncated %d chars]" % (len(s) - self.MAX_LEN)
        return s

    def _add(self, status, name, msg=""):
        name = self._trunc(name)
        msg = self._trunc(msg)
        self.results.append({"group": self._group, "name": name, "status": status, "msg": msg})
        if not self.quiet or status in ("FAIL", "CRASH"):
            line = "  [%s] %s" % (status, name)
            if msg:
                line += "  -- %s" % msg
            print(_c(status, line))

    def ok(self, name, msg=""):
        self._add("PASS", name, msg)

    def fail(self, name, msg=""):
        self._add("FAIL", name, msg)

    def skip(self, name, msg=""):
        self._add("SKIP", name, msg)

    def crash(self, name, msg=""):
        self._add("CRASH", name, msg)

    def info(self, name, msg=""):
        self._add("INFO", name, msg)

    def check(self, name, condition, ok_msg="", fail_msg=""):
        """Shorthand: record PASS/FAIL depending on a boolean condition."""
        if condition:
            self.ok(name, ok_msg)
        else:
            self.fail(name, fail_msg)
        return condition

    def counts(self):
        c = {"PASS": 0, "FAIL": 0, "SKIP": 0, "CRASH": 0, "INFO": 0}
        for r in self.results:
            c[r["status"]] = c.get(r["status"], 0) + 1
        return c

    def print_summary(self):
        c = self.counts()
        print("\n" + "=" * 60)
        print("SUMMARY: %d passed, %d failed, %d skipped, %d crashes"
              % (c["PASS"], c["FAIL"], c["SKIP"], c["CRASH"]))
        print("=" * 60)
        if c["FAIL"] or c["CRASH"]:
            print("\nFailures:")
            for r in self.results:
                if r["status"] in ("FAIL", "CRASH"):
                    print("  [%s][%s] %s%s" % (
                        r["status"], r["group"], r["name"],
                        ("  -- " + r["msg"]) if r["msg"] else ""))

    def write_markdown(self, path):
        c = self.counts()
        with open(path, "w") as f:
            f.write("# webserv tester report\n\n")
            f.write("Generated: %s\n\n" % time.strftime("%Y-%m-%d %H:%M:%S"))
            f.write("**%d passed, %d failed, %d skipped, %d crashes**\n\n"
                    % (c["PASS"], c["FAIL"], c["SKIP"], c["CRASH"]))
            current_group = None
            for r in self.results:
                if r["group"] != current_group:
                    current_group = r["group"]
                    f.write("\n## %s\n\n" % current_group)
                icon = {"PASS": "PASS", "FAIL": "**FAIL**", "SKIP": "SKIP",
                        "CRASH": "**CRASH**", "INFO": "info"}[r["status"]]
                f.write("- [%s] %s%s\n" % (icon, r["name"],
                        ("  \n  " + r["msg"]) if r["msg"] else ""))

    def exit_code(self):
        c = self.counts()
        return 1 if (c["FAIL"] or c["CRASH"]) else 0
