# webserv_tester

An extensive, external, black-box test suite for the `webserv` project
(the 42 school HTTP-server project) living in the parent directory. It
drives the already-compiled `../webserv` binary over real TCP sockets --
the same way `curl`, `telnet`, `nc`, a browser, or an evaluator would --
and **never modifies anything under the main project** (`src/`,
`include/`, `www/`, `config.conf`, `Makefile`, ...). Everything the suite
needs (test configs, test document roots, CGI scripts) lives inside this
`webserv_tester/` folder.

It is organized around the two source documents for this project:
`docs/en.webserver.pdf` (the subject) and `docs/webservevalsheet.html`
(the peer-evaluation scale), so the checks map directly onto what an
evaluator will actually do during your defense.

## Requirements

- Python 3 (standard library only -- no `pip install` needed).
- The project must compile (`run_tests.py` runs `make` for you unless you
  pass `--no-build`).
- Optional, for extra coverage: `siege` (`sudo apt-get install siege`,
  exactly as the evalsheet suggests), `curl`, `valgrind`.

## Running it

```bash
cd webserv_tester
python3 run_tests.py                    # full quick run (~1-2 minutes)
python3 run_tests.py --stress           # also run the heavier load test
python3 run_tests.py --only cgi         # only modules whose name matches
python3 run_tests.py --skip stress      # skip modules whose name matches
python3 run_tests.py --report out.md    # also dump a Markdown report
python3 run_tests.py --no-build         # skip the `make` step
```

Exit code is `0` iff every check passed (skips/infos don't count against
it). A run takes longer than you might expect for a test suite this
size **because of a confirmed bug** (see below): many malformed-request
checks intentionally wait out a short timeout to prove the server never
responds. That's the point of those checks, not a slow test harness.

## Layout

```
webserv_tester/
  run_tests.py          entry point / orchestrator
  static_checks.py       grep-based heuristics for the evalsheet's
                          "check the code" section (single poll(), no
                          errno-driven control flow, fork() only for CGI,
                          Makefile rules/flags, README structure, ...)
  lib/
    httpclient.py         raw-socket HTTP/1.x client (deliberately NOT
                           `requests`/`http.client`, so it can send
                           malformed/partial/pipelined bytes on the wire)
    procman.py             start/stop webserv, crash detection, RSS sampling
    report.py               PASS/FAIL/SKIP/CRASH/INFO recorder + summary
  configs/                 config files used ONLY by this test suite
  www/                     document roots + CGI scripts used ONLY by this
                           test suite (site_default/, site_b/, cgi-bin/, ...)
  tests/
    test_startup.py               CLI args, invalid/missing config handling
    test_basic_http.py            GET/POST/DELETE, status codes, allow_methods
    test_protocol_edgecases.py    headers, chunked encoding, keep-alive, HTTP versions
    test_body_limits.py           client_max_body_size enforcement
    test_cgi.py                   CGI env vars, GET/POST, error handling
    test_known_gaps.py            redirect / error_page / autoindex / upload_store
    test_security.py              path traversal and other weird-URI robustness
    test_port_and_process.py      multi-port, shared host:port, competing processes
    test_bonus.py                 cookies/session, multiple CGI types (informational)
    test_stress.py                concurrency, availability, memory stability
  logs/                    per-run server stdout/stderr (one file per test group)
  tmp/                     scratch files the tests create/delete
```

## What this already found (as of this writing)

Running the suite against the current build turned up several concrete,
reproducible issues -- ranked roughly by how badly they'd hurt you in a
live defense. Every one of them was independently double-checked by hand
with `curl`/`nc`/raw sockets, not just trusted from the Python client.

1. **Any invalid/missing config file makes the server hang forever**
   instead of exiting with an error. `ConfigParser::parseFile()` returns
   `void` and just `return`s early on failure; `main.cpp` never checks
   whether parsing actually succeeded, so it happily builds a `Server`
   with zero parsed servers and calls `run()`, which blocks in `poll()`
   forever. Reproduce: `./webserv does_not_exist.conf` (or any
   syntactically broken config) -- it prints an error line, then
   `Server started with 0 listening socket(s)`, then never returns.
   This is likely the single worst finding here: it's the *first* thing
   many evaluators try.

2. **Malformed requests that the parser itself rejects (400/411/413/414/
   501/505) are logged but never answered, and the connection is never
   closed** -- the client just hangs. Confirmed for: missing/duplicate/
   empty `Host` on HTTP/1.1, lowercase or unknown methods, bad HTTP
   version, request-line syntax errors, URIs over 2048 bytes, POST with
   no `Content-Length`/`Transfer-Encoding`, mismatched duplicate
   `Content-Length`, `Content-Length` + `Transfer-Encoding` together,
   unsupported `Transfer-Encoding`, malformed chunk sizes/data, bodies
   over `client_max_body_size` (both plain and chunked). This violates
   the subject's explicit "a request to your server should never hang
   indefinitely." (`grep -n "HTTP parse error" src -r` after running a
   bad request shows the exception is caught and *logged*, but no
   response is ever written back to the client socket.)

3. **Keep-alive connections are broken after the first request.** Send
   two or more requests for *different* resources on the same
   persistent HTTP/1.1 connection, and every response after the first
   one replays the FIRST request's body, ignoring the new request line
   entirely. Since browsers reuse connections by default, this will
   visibly break real multi-resource page loads during the "Check with
   a browser" evaluation step.

4. **`Connection: close` is announced in the response header but the
   socket is never actually closed** afterward -- a strict HTTP/1.0 or
   `Connection: close` client is left hanging.

5. **Path traversal is not contained.** `GET`/`DELETE` with `../`
   segments in the URI are not normalized/sandboxed against the
   configured root (`path = locationRoot + uri` with no containment
   check), so a request can read or delete files outside the configured
   document root. Verified safely against disposable canary files (not
   your real filesystem) by `test_security.py`.

6. **Several config directives parse and validate successfully but are
   never applied at request time**: `return CODE URL;` (redirect --
   `src/core/Router.cpp` is empty), `error_page CODE path;` (the custom
   page content is never read outside the parser), `autoindex on;` on a
   directory with no index (the autoindex-generation call is commented
   out in `GetHandler`, so it's always 403 regardless), and
   `upload_store path;` (`PostHandler` always writes under
   `locationRoot + URI`, never under the configured store).

7. **CGI is not run in its own directory** (`CgiHandler::executeChild`
   never calls `chdir()`), so a CGI script that opens a sibling file by
   relative path will fail -- contradicting the subject's explicit "The
   CGI should be run in the correct directory for relative path file
   access."

8. **`CONTENT_TYPE` never reaches the CGI on POST**, because
   `CgiHandler::buildEnv` looks up `req.getHeader("Content-Type")`
   (capitalized) while the request parser stores all header names
   lower-cased, so the `std::map` lookup always misses.

What's solid: `GET`/`POST`/`DELETE` on well-formed requests, multipart
and urlencoded uploads, multiple ports serving different sites,
`allow_methods` (405) enforcement, chunked-request un-chunking, CGI
GET/POST including large/no-`Content-Length`/misbehaving scripts (error,
empty output, no header terminator) without ever crashing the server,
non-blocking behaviour under a slow/drip client and under a slow CGI
running concurrently with other clients, the Makefile and compile flags,
and raw throughput/availability/memory stability under sustained
concurrent load (>2500 req/s, 100% availability, flat RSS in the runs
done while building this suite).

Re-run `python3 run_tests.py` after fixing things -- every finding above
has a corresponding automated check that will flip to PASS once it's
fixed, so you don't have to take this file's word for it.
