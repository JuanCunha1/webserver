"""CLI argument handling + config validation, per subject IV ("Arguments")
and the evalsheet's crash-detection rule: an invalid config must produce a
clean, immediate, non-zero exit -- never a hang and never a crash.

CONFIRMED ROOT CAUSE for every "rejected cleanly" check below that fails:
ConfigParser::parseFile() (src/config/ConfigParser.cpp) returns `void` and
simply `return`s early on any failure (file not found, bad braces, parse
error, failed semantic validation...), leaving `_servers` empty. main.cpp
never checks whether parsing actually succeeded -- it unconditionally
does `Server server(parser.getServers()); server.start(); server.run();`.
With zero parsed servers this creates a Server with 0 listening sockets
and then calls run(), which blocks in poll() forever. So: ANY invalid,
missing, or unparseable config file makes webserv hang immediately
instead of printing a usage/parse error and exiting -- confirmed by hand
with `./webserv configs/does_not_exist.conf` (prints "Error: Cannot open
file ...", then "Server started with 0 listening socket(s)", then just
sits there until killed).
"""
import os
import subprocess

HANG_NOTE = (
    "process never exited (killed after timeout) -- confirmed root cause: "
    "ConfigParser::parseFile() returns void and just `return`s on failure, "
    "main.cpp never checks it, so Server is built with 0 parsed servers "
    "and run() blocks in poll() forever instead of exiting with an error"
)


def _run_once(ctx, args, timeout=2.0):
    try:
        p = subprocess.run([ctx["binary"]] + args, cwd=ctx["tester_dir"],
                            capture_output=True, text=True, timeout=timeout)
        return p.returncode, p.stdout, p.stderr
    except subprocess.TimeoutExpired:
        return None, "", None


def _check_rejected_cleanly(rec, ctx, name, args):
    rc, out, err = _run_once(ctx, args)
    if rc is None:
        rec.fail(name, "CRITICAL: " + HANG_NOTE)
    else:
        rec.check(name, rc != 0, "rc=%r" % rc,
                  "config was WRONGLY accepted (rc=%r, stdout=%r, stderr=%r)" % (rc, out, err))


def run(rec, ctx):
    rec.group("Startup / argument / config-file handling")

    rc, out, err = _run_once(ctx, [])
    rec.check("no arguments -> prints usage and exits with a non-zero code",
              rc is not None and rc != 0,
              "rc=%r" % rc, "rc=%r stdout=%r stderr=%r" % (rc, out, err))

    rc, out, err = _run_once(ctx, ["configs/basic.conf", "extra_arg"])
    rec.check("too many arguments -> exits cleanly (no crash, no hang)",
              rc is not None,
              "rc=%r" % rc, HANG_NOTE if rc is None else "rc=%r" % rc)

    _check_rejected_cleanly(rec, ctx, "nonexistent config file -> rejected cleanly, non-zero exit",
                            ["configs/does_not_exist.conf"])

    garbage_path = os.path.join(ctx["tester_dir"], "tmp", "garbage.conf")
    with open(garbage_path, "w") as f:
        f.write("this is not { a valid ;; config ] file at all\n")
    _check_rejected_cleanly(rec, ctx, "syntactically invalid config -> rejected cleanly, non-zero exit",
                            ["tmp/garbage.conf"])

    empty_path = os.path.join(ctx["tester_dir"], "tmp", "empty.conf")
    with open(empty_path, "w") as f:
        f.write("")
    _check_rejected_cleanly(rec, ctx, "empty config file -> rejected cleanly (no servers to run)",
                            ["tmp/empty.conf"])

    _check_rejected_cleanly(rec, ctx, "duplicate `location /` in one server -> rejected cleanly, non-zero exit",
                            ["configs/duplicate_location.conf"])

    _check_rejected_cleanly(rec, ctx,
                            "two servers sharing host:port with no server_name -> rejected cleanly, non-zero exit",
                            ["configs/vhost_conflict.conf"])

    bad_root_path = os.path.join(ctx["tester_dir"], "tmp", "bad_root.conf")
    with open(bad_root_path, "w") as f:
        f.write("server {\n    listen 8599;\n    host 127.0.0.1;\n"
                 "    root www/this_directory_does_not_exist;\n    index index.html;\n"
                 "    location / { allow_methods GET; }\n}\n")
    _check_rejected_cleanly(rec, ctx, "config with a nonexistent root directory -> rejected cleanly, non-zero exit",
                            ["tmp/bad_root.conf"])
