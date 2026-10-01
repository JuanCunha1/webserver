"""Static, grep-based heuristics mirroring the evalsheet's "Check the code
and ask questions" section. These are NOT a substitute for actually
reading the code with the group during the defense -- they are fast
sanity nets that flag obvious violations (banned functions, errno usage
right after I/O, more than one poll()-family call, fork() used outside
CGI, missing Makefile rules/flags) so you know where to look closely.

Heuristic by nature: a grep can false-positive on a comment, or miss a
clever violation. Treat FAIL here as "go read this file", not as a final
verdict.
"""
import os
import re
import subprocess


POLL_FAMILY = ("poll(", "select(", "epoll_wait(", "epoll_create(", "kqueue(")

BANNED_TOKENS = [
    # Anything not in the subject's "External Function" allow-list.
    "system(", "popen(",
]

ALLOWED_FUNCS = {
    "execve", "pipe", "strerror", "gai_strerror", "errno", "dup", "dup2",
    "fork", "socketpair", "htons", "htonl", "ntohs", "ntohl", "select",
    "poll", "epoll_create", "epoll_ctl", "epoll_wait", "kqueue", "kevent",
    "socket", "accept", "listen", "send", "recv", "shutdown", "chdir",
    "bind", "connect", "getaddrinfo", "freeaddrinfo", "setsockopt",
    "getsockname", "getprotobyname", "fcntl", "close", "read", "write",
    "waitpid", "kill", "signal", "access", "stat", "open", "opendir",
    "readdir", "closedir",
}


def _iter_source_files(src_dir, include_headers_dir=None):
    exts = (".cpp", ".hpp", ".h", ".tpp", ".ipp")
    for base in filter(None, [src_dir, include_headers_dir]):
        for root, _dirs, files in os.walk(base):
            for fn in files:
                if fn.endswith(exts) and not fn.endswith(":Zone.Identifier"):
                    yield os.path.join(root, fn)


def _read(path):
    with open(path, "r", errors="replace") as f:
        return f.read()


def run(rec, ctx):
    rec.group("Static code review (heuristic, per evalsheet 'Check the code')")
    src_dir = os.path.join(ctx["root"], "src")
    inc_dir = os.path.join(ctx["root"], "include")
    files = list(_iter_source_files(src_dir, inc_dir))

    if not files:
        rec.fail("locate source files", "no .cpp/.hpp found under src/ or include/")
        return

    # 1. Single poll()-family call in the whole project (the subject demands
    #    exactly one poll()/select()/epoll_wait()/kqueue() loop driving all I/O).
    poll_hits = []
    for path in files:
        text = _read(path)
        for lineno, line in enumerate(text.splitlines(), 1):
            stripped = line.strip()
            if stripped.startswith("//") or stripped.startswith("*"):
                continue
            # Strip string/char literals so error messages like
            # "poll() failed" don't get counted as a second call site.
            code_only = re.sub(r'"(?:[^"\\]|\\.)*"', '""', line)
            code_only = re.sub(r"'(?:[^'\\]|\\.)*'", "''", code_only)
            for token in ("poll(", "select(", "epoll_wait(", "kevent("):
                if token in code_only:
                    poll_hits.append((path, lineno, token.rstrip("(")))
    rec.info("poll()-family call sites found", "; ".join(
        "%s:%d %s" % (os.path.relpath(p, ctx["root"]), n, t) for p, n, t in poll_hits) or "none")
    rec.check(
        "exactly one poll()/select()/epoll_wait()/kevent() call site",
        len(poll_hits) == 1,
        "single call site, as required",
        "found %d call sites -- subject requires exactly 1 (grade is 0 if not)" % len(poll_hits))

    # 2. errno inspected right after read/recv/write/send (forbidden).
    errno_after_io = []
    io_calls = ("read(", "recv(", "write(", "send(")
    for path in files:
        lines = _read(path).splitlines()
        for i, line in enumerate(lines):
            if any(c in line for c in io_calls) and "errno" not in line:
                window = "\n".join(lines[i + 1:i + 4])
                if re.search(r"\berrno\b", window):
                    errno_after_io.append((path, i + 1))
    if errno_after_io:
        detail = "; ".join("%s:%d" % (os.path.relpath(p, ctx["root"]), n) for p, n in errno_after_io[:10])
        rec.fail("no errno-driven control flow after read/recv/write/send",
                  "possible occurrences (verify manually, grep is naive): " + detail)
    else:
        rec.ok("no errno-driven control flow after read/recv/write/send (heuristic)")

    # 3. fork() only used for CGI.
    fork_sites = []
    for path in files:
        text = _read(path)
        if "fork(" in text:
            fork_sites.append(os.path.relpath(path, ctx["root"]))
    non_cgi_fork = [p for p in fork_sites if "cgi" not in p.lower()]
    rec.info("fork() call sites", ", ".join(fork_sites) or "none")
    rec.check("fork() only appears in CGI-related files (heuristic on filename)",
              not non_cgi_fork,
              "", "fork() found outside CGI-named files: %s" % ", ".join(non_cgi_fork))

    # 4. execve() never used to spawn another web server (can't verify intent,
    #    just report where it's used so it can be eyeballed).
    execve_sites = [os.path.relpath(p, ctx["root"]) for p in files if "execve(" in _read(p)]
    rec.info("execve() call sites (verify none launches another web server)",
             ", ".join(execve_sites) or "none")

    # 5. Banned functions / obviously-disallowed external calls.
    banned_hits = []
    for path in files:
        text = _read(path)
        for tok in BANNED_TOKENS:
            if tok in text:
                banned_hits.append((os.path.relpath(path, ctx["root"]), tok))
    rec.check("no obviously-banned functions (system/popen)", not banned_hits,
              "", "found: %s" % banned_hits)

    # 6. Makefile sanity.
    makefile = os.path.join(ctx["root"], "Makefile")
    if not os.path.isfile(makefile):
        rec.fail("Makefile exists", "no Makefile at project root")
    else:
        mk = _read(makefile)
        for rule in ("$(NAME)", "all", "clean", "fclean", "re"):
            pattern = re.compile(r"(?m)^%s\s*:" % re.escape(rule) if rule != "$(NAME)"
                                  else r"(?m)^\$\(NAME\)\s*:")
            rec.check("Makefile has rule '%s'" % rule, bool(pattern.search(mk)))
        for flag in ("-Wall", "-Wextra", "-Werror"):
            rec.check("Makefile compiles with %s" % flag, flag in mk)
        rec.check("Makefile targets C++98 (-std=c++98)", "c++98" in mk)

    # 7. No relinking: `make` then `make` again should not recompile/relink.
    try:
        subprocess.run(["make", "-C", ctx["root"], "-j", "1"], check=False,
                        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=120)
        second = subprocess.run(["make", "-C", ctx["root"]], check=False,
                                 capture_output=True, text=True, timeout=120)
        relinked = "Nothing to be done" not in second.stdout and second.stdout.strip() != ""
        rec.check("`make` twice in a row does not relink/recompile",
                  not relinked, "", "second `make` run produced output:\n%s" % second.stdout)
    except (OSError, subprocess.SubprocessError) as e:
        rec.skip("`make` re-run relinking check", str(e))

    # 8. -std=c++98 actually compiles clean (subject explicitly requires this
    #    to still work even though the default Makefile might already pass it).
    try:
        probe = subprocess.run(
            ["c++", "-Wall", "-Wextra", "-Werror", "-std=c++98", "-Iinclude", "-fsyntax-only",
             os.path.join("src", "main.cpp")],
            cwd=ctx["root"], capture_output=True, text=True, timeout=60)
        rec.check("src/main.cpp syntax-checks clean under -std=c++98 -Wall -Wextra -Werror",
                  probe.returncode == 0, "", probe.stderr[-2000:])
    except (OSError, subprocess.SubprocessError) as e:
        rec.skip("c++98 syntax-only probe", str(e))

    # 9. README.md presence + required structure (subject Chapter V).
    readme_path = os.path.join(ctx["root"], "README.md")
    if not os.path.isfile(readme_path):
        rec.fail("README.md exists at repo root", "missing")
    else:
        content = _read(readme_path)
        first_line = content.splitlines()[0].strip() if content.splitlines() else ""
        looks_italic_intro = (
            (first_line.startswith("_") and first_line.endswith("_")) or
            (first_line.startswith("*") and first_line.endswith("*"))
        ) and "42 curriculum" in first_line
        rec.check("README first line is italicized '...created as part of the 42 curriculum by <logins>'",
                   looks_italic_intro, first_line, "got: %r" % first_line)
        for section in ("Description", "Instructions", "Resources"):
            rec.check("README has a '%s' section" % section,
                      re.search(r"^#+\s*%s" % section, content, re.MULTILINE) is not None)
        rec.check("README mentions AI usage in Resources",
                  bool(re.search(r"\bAI\b", content)),
                  "", "README should describe how AI was used, per subject Chapter V")
