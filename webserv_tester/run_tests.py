#!/usr/bin/env python3
"""Extensive external test suite for the `webserv` project (42's HTTP
server project). Does NOT modify anything under the project's own
src/, include/, www/, conf/ or config.conf -- everything here lives in
this webserv_tester/ folder and drives the already-compiled `webserv`
binary as a black box, the way an evaluator would with curl/telnet/nc/
siege plus a browser.

Usage:
    python3 run_tests.py                 # quick run (a few seconds/group)
    python3 run_tests.py --stress        # also run the heavier load test
    python3 run_tests.py --only cgi      # only run modules matching 'cgi'
    python3 run_tests.py --skip stress   # skip modules matching 'stress'
    python3 run_tests.py --no-build      # don't run `make` first
    python3 run_tests.py --report out.md # also write a Markdown report

See README.md in this folder for the full picture (what's covered, and
the concrete bugs this suite already found in this codebase).
"""
import argparse
import importlib
import os
import subprocess
import sys
import time

TESTER_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.dirname(TESTER_DIR)

sys.path.insert(0, TESTER_DIR)
from lib.report import Recorder  # noqa: E402

MODULES = [
    "static_checks",
    "tests.test_startup",
    "tests.test_basic_http",
    "tests.test_protocol_edgecases",
    "tests.test_body_limits",
    "tests.test_cgi",
    "tests.test_known_gaps",
    "tests.test_security",
    "tests.test_port_and_process",
    "tests.test_bonus",
    "tests.test_stress",
]


def build_project(rec):
    print("\n>> make -C %s" % PROJECT_ROOT)
    try:
        p = subprocess.run(["make", "-C", PROJECT_ROOT], capture_output=True, text=True, timeout=180)
        sys.stdout.write(p.stdout)
        sys.stderr.write(p.stderr)
        if p.returncode != 0:
            rec.group("Build")
            rec.crash("`make` in project root", "returncode=%d\n%s" % (p.returncode, p.stderr[-2000:]))
            return False
        return True
    except (OSError, subprocess.SubprocessError) as e:
        rec.group("Build")
        rec.crash("`make` in project root", str(e))
        return False


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--stress", action="store_true", help="run the heavier/longer stress variant")
    ap.add_argument("--only", default=None, help="only run modules whose name contains this substring")
    ap.add_argument("--skip", default=None, help="skip modules whose name contains this substring")
    ap.add_argument("--no-build", action="store_true", help="don't run `make` before testing")
    ap.add_argument("--binary", default=os.path.join(PROJECT_ROOT, "webserv"),
                     help="path to the webserv binary")
    ap.add_argument("--report", default=None, help="write a Markdown report to this path")
    args = ap.parse_args()

    rec = Recorder()

    if not args.no_build:
        if not build_project(rec):
            rec.print_summary()
            sys.exit(2)

    if not os.path.isfile(args.binary):
        print("ERROR: webserv binary not found at %s (build it first, or pass --binary)" % args.binary)
        sys.exit(2)

    os.makedirs(os.path.join(TESTER_DIR, "logs"), exist_ok=True)
    os.makedirs(os.path.join(TESTER_DIR, "tmp"), exist_ok=True)

    ctx = {
        "root": PROJECT_ROOT,
        "tester_dir": TESTER_DIR,
        "binary": args.binary,
        "log_dir": os.path.join(TESTER_DIR, "logs"),
        "stress": args.stress,
    }

    t0 = time.time()
    for mod_name in MODULES:
        if args.only and args.only not in mod_name:
            continue
        if args.skip and args.skip in mod_name:
            continue
        mod = importlib.import_module(mod_name)
        try:
            mod.run(rec, ctx)
        except Exception as e:  # a bug in the TEST ITSELF, not the server
            rec.group(mod_name + " (tester internal error)")
            rec.fail("test module raised an unexpected exception", repr(e))
            import traceback
            traceback.print_exc()

    elapsed = time.time() - t0
    print("\n(total time: %.1fs)" % elapsed)
    rec.print_summary()

    if args.report:
        rec.write_markdown(args.report)
        print("\nMarkdown report written to %s" % args.report)

    sys.exit(rec.exit_code())


if __name__ == "__main__":
    main()
