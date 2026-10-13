# webserv tester report

Generated: 2026-09-29 10:41:29

**94 passed, 45 failed, 2 skipped, 0 crashes**


## Static code review (heuristic, per evalsheet 'Check the code')

- [info] poll()-family call sites found  
  src/network/Server.cpp:106 poll
- [PASS] exactly one poll()/select()/epoll_wait()/kevent() call site  
  single call site, as required
- [PASS] no errno-driven control flow after read/recv/write/send (heuristic)
- [info] fork() call sites  
  src/protocol/handlers/CgiHandler.cpp
- [PASS] fork() only appears in CGI-related files (heuristic on filename)
- [info] execve() call sites (verify none launches another web server)  
  src/protocol/handlers/CgiHandler.cpp
- [PASS] no obviously-banned functions (system/popen)
- [PASS] Makefile has rule '$(NAME)'
- [PASS] Makefile has rule 'all'
- [PASS] Makefile has rule 'clean'
- [PASS] Makefile has rule 'fclean'
- [PASS] Makefile has rule 're'
- [PASS] Makefile compiles with -Wall
- [PASS] Makefile compiles with -Wextra
- [PASS] Makefile compiles with -Werror
- [PASS] Makefile targets C++98 (-std=c++98)
- [PASS] `make` twice in a row does not relink/recompile
- [PASS] src/main.cpp syntax-checks clean under -std=c++98 -Wall -Wextra -Werror
- [**FAIL**] README first line is italicized '...created as part of the 42 curriculum by <logins>'  
  got: '# webserver'
- [**FAIL**] README has a 'Description' section
- [**FAIL**] README has a 'Instructions' section
- [**FAIL**] README has a 'Resources' section
- [**FAIL**] README mentions AI usage in Resources  
  README should describe how AI was used, per subject Chapter V

## Startup / argument / config-file handling

- [PASS] no arguments -> prints usage and exits with a non-zero code  
  rc=1
- [PASS] too many arguments -> exits cleanly (no crash, no hang)  
  rc=1
- [**FAIL**] nonexistent config file -> rejected cleanly, non-zero exit  
  CRITICAL: process never exited (killed after timeout) -- confirmed root cause: ConfigParser::parseFile() returns void and just `return`s on failure, main.cpp never checks it, so Server is built with 0 parsed servers and run() blocks in poll() forever instead of exiting with an error
- [**FAIL**] syntactically invalid config -> rejected cleanly, non-zero exit  
  CRITICAL: process never exited (killed after timeout) -- confirmed root cause: ConfigParser::parseFile() returns void and just `return`s on failure, main.cpp never checks it, so Server is built with 0 parsed servers and run() blocks in poll() forever instead of exiting with an error
- [**FAIL**] empty config file -> rejected cleanly (no servers to run)  
  CRITICAL: process never exited (killed after timeout) -- confirmed root cause: ConfigParser::parseFile() returns void and just `return`s on failure, main.cpp never checks it, so Server is built with 0 parsed servers and run() blocks in poll() forever instead of exiting with an error
- [**FAIL**] duplicate `location /` in one server -> rejected cleanly, non-zero exit  
  CRITICAL: process never exited (killed after timeout) -- confirmed root cause: ConfigParser::parseFile() returns void and just `return`s on failure, main.cpp never checks it, so Server is built with 0 parsed servers and run() blocks in poll() forever instead of exiting with an error
- [**FAIL**] two servers sharing host:port with no server_name -> rejected cleanly, non-zero exit  
  CRITICAL: process never exited (killed after timeout) -- confirmed root cause: ConfigParser::parseFile() returns void and just `return`s on failure, main.cpp never checks it, so Server is built with 0 parsed servers and run() blocks in poll() forever instead of exiting with an error
- [**FAIL**] config with a nonexistent root directory -> rejected cleanly, non-zero exit  
  CRITICAL: process never exited (killed after timeout) -- confirmed root cause: ConfigParser::parseFile() returns void and just `return`s on failure, main.cpp never checks it, so Server is built with 0 parsed servers and run() blocks in poll() forever instead of exiting with an error

## Basic HTTP: GET / POST / DELETE, status codes, allowed methods

- [PASS] GET / -> 200
- [PASS] GET / body contains index marker
- [PASS] GET / has Content-Length header
- [PASS] Content-Length matches actual body size
- [PASS] GET / has Content-Type header
- [PASS] GET /page.html -> 200 with correct body
- [PASS] GET nested static file -> 200
- [PASS] GET missing file -> 404
- [PASS] 404 response reason phrase is 'Not Found'
- [PASS] GET a directory with no index file -> 403 (no autoindex configured here)
- [**FAIL**] PUT (unsupported but syntactically valid method) -> expected 501  
  SERVER HUNG: no response and connection not closed for: PUT (unsupported but syntactically valid method) -> expected 501 -- violates 'a request should never hang indefinitely'
- [**FAIL**] PATCH -> expected 501  
  SERVER HUNG: no response and connection not closed for: PATCH -> expected 501 -- violates 'a request should never hang indefinitely'
- [**FAIL**] lowercase method 'get' -> expected 400 Bad Request  
  SERVER HUNG: no response and connection not closed for: lowercase method 'get' -> expected 400 Bad Request -- violates 'a request should never hang indefinitely'
- [**FAIL**] bogus HTTP version 'HTTP/9.9' -> expected 505  
  SERVER HUNG: no response and connection not closed for: bogus HTTP version 'HTTP/9.9' -> expected 505 -- violates 'a request should never hang indefinitely'
- [**FAIL**] extra token on the request line -> expected 400  
  SERVER HUNG: no response and connection not closed for: extra token on the request line -> expected 400 -- violates 'a request should never hang indefinitely'
- [**FAIL**] URI not starting with '/' -> expected 400  
  SERVER HUNG: no response and connection not closed for: URI not starting with '/' -> expected 400 -- violates 'a request should never hang indefinitely'
- [**FAIL**] URI longer than 2048 bytes -> expected 414  
  SERVER HUNG: no response and connection not closed for: URI longer than 2048 bytes -> expected 414 -- violates 'a request should never hang indefinitely'
- [PASS] GET on GET-only route -> 200
- [PASS] POST on GET-only route -> 405 Method Not Allowed
- [PASS] DELETE on GET-only route -> 405 Method Not Allowed
- [info] DELETE / (root dir, allow_methods includes DELETE) response  
  <Response 403 Forbidden, 4 headers, 48 body bytes>
- [PASS] POST new file -> 201 Created
- [PASS] 201 response has Location header
- [PASS] GET the just-uploaded file -> 200 with matching body
- [PASS] POST to an EXISTING file -> 200 OK (not 201)
- [PASS] DELETE existing file -> 204 No Content
- [PASS] GET after DELETE -> 404
- [PASS] DELETE an already-missing file -> 404 (not 500/403)
- [PASS] multipart/form-data upload -> 201 Created
- [PASS] GET the multipart-uploaded file back -> 200 with identical bytes
- [PASS] application/x-www-form-urlencoded POST -> 2xx, no crash
- [**FAIL**] POST with no Content-Length/Transfer-Encoding -> expected 411 Length Required  
  SERVER HUNG: no response and connection not closed for: POST with no Content-Length/Transfer-Encoding -> expected 411 Length Required -- violates 'a request should never hang indefinitely'
- [PASS] DELETE a directory -> 403 Forbidden (not removed, no crash)
- [PASS] directory survives the attempted DELETE
- [PASS] port 8500 and 8501 serve DIFFERENT content (distinct websites)
- [PASS] secondary site (8501) has its own marker
- [PASS] server still alive after this whole test group

## HTTP/1.x protocol edge cases (headers, chunked, keep-alive, malformed input)

- [**FAIL**] HTTP/1.1 request with NO Host header -> expected 400  
  SERVER HUNG: no response and connection not closed for: HTTP/1.1 request with NO Host header -> expected 400 -- violates 'a request should never hang indefinitely'
- [**FAIL**] HTTP/1.1 request with EMPTY Host header -> expected 400  
  SERVER HUNG: no response and connection not closed for: HTTP/1.1 request with EMPTY Host header -> expected 400 -- violates 'a request should never hang indefinitely'
- [**FAIL**] HTTP/1.1 request with TWO Host headers -> expected 400  
  SERVER HUNG: no response and connection not closed for: HTTP/1.1 request with TWO Host headers -> expected 400 -- violates 'a request should never hang indefinitely'
- [PASS] HTTP/1.0 request with NO Host header -> allowed (200)
- [**FAIL**] header line with no colon at all -> expected 400  
  SERVER HUNG: no response and connection not closed for: header line with no colon at all -> expected 400 -- violates 'a request should never hang indefinitely'
- [**FAIL**] space before the colon in a header line -> expected 400  
  SERVER HUNG: no response and connection not closed for: space before the colon in a header line -> expected 400 -- violates 'a request should never hang indefinitely'
- [**FAIL**] control character in header name -> expected 400  
  SERVER HUNG: no response and connection not closed for: control character in header name -> expected 400 -- violates 'a request should never hang indefinitely'
- [PASS] header value containing spaces is accepted (200)
- [PASS] identical duplicated Content-Length headers -> tolerated, expected 2xx
- [**FAIL**] duplicated Content-Length headers with DIFFERENT values -> expected 400  
  SERVER HUNG: no response and connection not closed for: duplicated Content-Length headers with DIFFERENT values -> expected 400 -- violates 'a request should never hang indefinitely'
- [**FAIL**] Content-Length AND Transfer-Encoding both present -> expected 400  
  SERVER HUNG: no response and connection not closed for: Content-Length AND Transfer-Encoding both present -> expected 400 -- violates 'a request should never hang indefinitely'
- [**FAIL**] unsupported Transfer-Encoding value -> expected 501  
  SERVER HUNG: no response and connection not closed for: unsupported Transfer-Encoding value -> expected 501 -- violates 'a request should never hang indefinitely'
- [PASS] well-formed chunked upload -> 201 Created
- [PASS] un-chunked body was reassembled correctly ('Wikipedia')
- [**FAIL**] chunk size that isn't valid hex -> expected 400  
  SERVER HUNG: no response and connection not closed for: chunk size that isn't valid hex -> expected 400 -- violates 'a request should never hang indefinitely'
- [**FAIL**] chunk data missing its trailing CRLF -> expected 400  
  SERVER HUNG: no response and connection not closed for: chunk data missing its trailing CRLF -> expected 400 -- violates 'a request should never hang indefinitely'
- [**FAIL**] bare LF (no CR) as line ending -> expected 400  
  SERVER HUNG: no response and connection not closed for: bare LF (no CR) as line ending -> expected 400 -- violates 'a request should never hang indefinitely'
- [PASS] explicit 'Connection: close' -> response says close
- [**FAIL**] server actually closes the TCP connection after a 'Connection: close' response  
  recv() timed out instead of returning EOF -- the header says 'close' but the socket is left open, so an HTTP/1.0-only or Connection:-close client would hang
- [PASS] keep-alive: first request on the connection succeeds
- [**FAIL**] CRITICAL: on a keep-alive (persistent) connection, EACH subsequent request returns the resource actually requested (not a repeat of request #1)  
  3/3 follow-up requests on the SAME connection returned the WRONG resource: request #2 for /page.html got body of request #1 instead (marker mismatch); request #3 for /secretdir/hidden.txt got body of request #1 instead (marker mismatch); request #4 for /page.html got body of request #1 instead (mark... [truncated 200 chars]
- [info] pipelined requests both answered, in order  
  not supported / timed out (not mandatory per subject): TimeoutError('timed out')
- [PASS] server survives an abrupt disconnect mid-request (still serves new clients)
- [PASS] OTHER clients keep getting served WHILE a slow client trickles its request in (proves the single poll() loop isn't blocked by one slow reader)
- [PASS] the byte-at-a-time ('slowloris'-style) request itself eventually completes
- [PASS] server still alive after protocol edge-case barrage

## client_max_body_size enforcement

- [PASS] POST body UNDER the configured limit succeeds
- [**FAIL**] POST body OVER the configured limit (Content-Length) -> expected 413  
  SERVER HUNG: no response and connection not closed for: POST body OVER the configured limit (Content-Length) -> expected 413 -- violates 'a request should never hang indefinitely'
- [**FAIL**] evalsheet-style `curl -X POST --data <over-limit body>` -> 413  
  curl reported 000 (no response at all before --max-time) -- consistent with the 413-hang bug found above: the server never answers an over-limit POST
- [**FAIL**] chunked POST whose total size exceeds the limit -> expected 413  
  SERVER HUNG: no response and connection not closed for: chunked POST whose total size exceeds the limit -> expected 413 -- violates 'a request should never hang indefinitely'
- [PASS] server still alive after body-limit tests

## CGI (env vars, GET/POST, error resilience)

- [PASS] GET on a .py route invokes the CGI (200)
- [PASS] REQUEST_METHOD is exposed to the CGI
- [PASS] QUERY_STRING carries the client's query args to the CGI
- [PASS] SCRIPT_NAME/SCRIPT_FILENAME point at the requested script
- [info] optional CGI var PATH_INFO is not set  
  not strictly required by the subject, but common in real CGI gateways
- [info] optional CGI var SERVER_NAME is not set  
  not strictly required by the subject, but common in real CGI gateways
- [info] optional CGI var SERVER_PORT is not set  
  not strictly required by the subject, but common in real CGI gateways
- [info] optional CGI var REMOTE_ADDR is not set  
  not strictly required by the subject, but common in real CGI gateways
- [**FAIL**] CGI is executed with its CWD set to the script's own directory (subject IV.3: 'CGI should be run in the correct directory for relative path file access')  
  got "NOTFOUND:[Errno 2] No such file or directory: 'reldata.txt'" -- relative-path sibling file lookup failed from inside the CGI
- [PASS] POST on a .py route invokes the CGI (200)
- [PASS] POST body reaches the CGI on stdin, full and unmodified
- [PASS] CONTENT_LENGTH is exposed to the CGI for POST
- [**FAIL**] CONTENT_TYPE is exposed to the CGI for POST (watch for header lookup being case-sensitive against lower-cased stored header names)  
  got: CONTENT_TYPE=<MISSING>
- [PASS] chunked POST body is un-chunked before being handed to the CGI
- [PASS] CGI script that raises an uncaught exception -> clean HTTP error, not a hang/crash
- [PASS] CGI script producing ZERO output -> clean HTTP error, not a hang/crash
- [PASS] CGI output with no header/body separator at all -> clean HTTP error (e.g. 502), not a hang
- [PASS] CGI output with NO Content-Length -> body captured via EOF, per subject IV.3
- [PASS] large (~1MB) CGI output is relayed completely, unmodified
- [PASS] server survived every misbehaving-CGI scenario above without crashing
- [PASS] CGI 'Status:' header is relayed as the actual HTTP status code
- [PASS] CGI-provided Location header is relayed to the client
- [PASS] a fast request completes quickly WHILE a slow CGI (2s sleep) is still running
- [PASS] the slow CGI itself still completes successfully
- [PASS] server still alive after CGI tests

## Configured-but-possibly-not-wired-up features (redirect / error_page / autoindex / upload_store)

- [**FAIL**] location with `return 302 /index.html;` actually issues an HTTP redirect (subject IV.3: 'HTTP redirection' as a supported per-route config option)  
  got <Response 200 OK, 4 headers, 148 body bytes> instead of a 3xx + Location -- the parsed `return` directive (loc.returnRedirections) does not appear to be applied anywhere at request time
- [**FAIL**] `error_page 404 www/errors/404.html;` custom page is served instead of the built-in one  
  got the built-in generic 404 body instead of the configured custom page -- server.errorPages is parsed/validated but ErrorHandler::handleError() always builds its own generic HTML (still fine per subject: 'must have default error pages IF NONE ARE PROVIDED' -- here one WAS provided in config)
- [PASS] even the built-in default 404 page is at least well-formed HTML with 404 in it
- [**FAIL**] `autoindex on;` on a directory with no index file actually generates a directory listing (subject IV.3: 'Enabling or disabling directory listing')  
  got status=403 -- AutoIndex.cpp is present but empty and GetHandler's autoindex branch is commented out, so it falls through to 403 regardless of the autoindex setting
- [PASS] `autoindex off;` on the same kind of directory correctly returns 403 (not a listing)
- [info] POST to a route with `upload_store` configured  
  <Response 201 Created, 5 headers, 83 body bytes>
- [**FAIL**] uploaded file lands in the directory the route's `root` resolves to (here it coincides with `upload_store`, so this alone doesn't prove upload_store is honored -- PostHandler always writes to locationRoot+URI and never reads loc.uploadStore at all)  
  file not found where expected after POST
- [PASS] server still alive after known-gaps probing

## Security-adjacent robustness (path traversal, weird URIs)

- [**FAIL**] CRITICAL: GET with '../' path segments cannot escape the configured document root  
  path traversal via '..' successfully read a file OUTSIDE the web root (/../../tmp/canary_read.txt) -- ResponseBuilder builds `path = locationRoot + uri` with no normalization/containment check on '..' segments
- [**FAIL**] CRITICAL: DELETE with '../' path segments cannot delete files OUTSIDE the document root  
  DELETE via path traversal removed a file OUTSIDE the web root (status=204) -- on a real deployment this could delete arbitrary files the server process has permission to remove
- [PASS] NUL byte in the URI does not crash the server / hang the connection
- [PASS] weird-but-common URI form [//index.html] does not hang the server
- [PASS] weird-but-common URI form [/./index.html] does not hang the server
- [PASS] weird-but-common URI form [/index.html/] does not hang the server
- [PASS] weird-but-common URI form [/%2e%2e/index.html] does not hang the server
- [**FAIL**] weird-but-common URI form [very long query string (~5000 bytes, expected 414)] does not hang the server  
  SERVER HUNG: no response and connection not closed for: very long query string (~5000 bytes, expected 414) -- violates 'a request should never hang indefinitely'
- [PASS] server still alive after the security probes

## Port issues: multi-port, shared host:port, competing processes

- [PASS] second webserv process on an ALREADY-BOUND port exits cleanly (non-zero), doesn't hang  
  rc=1
- [PASS] the FIRST instance keeps running fine and is unaffected by the conflicting second one
- [PASS] first instance still serves requests normally after the conflict attempt
- [PASS] two server{} blocks sharing host:port (distinct server_name): if the group has no real vhost dispatch, the second bind() fails and the process must still exit CLEANLY (non-crash) rather than hang or segfault  
  rc=1

## Bonus: cookies/session, multiple CGI types (informational)

- [info] Set-Cookie present on a plain response (no session feature exercised yet)  
  absent
- [info] server echoes/uses a client-supplied Cookie header in any visible way  
  False
- [info] cookies/session support (bonus, subject Chapter VI)  
  no Set-Cookie observed on generic probes -- not a mandatory-part requirement; ask the team to demo their session example directly if they claim this bonus
- [info] php-cgi available on this machine to test a second CGI type  
  False
- [SKIP] multiple CGI types (bonus)  
  no second CGI interpreter (e.g. php-cgi) found on PATH

## Concurrency, availability and memory stability under load

- [info] custom load: 8 concurrent workers for 5s  
  18154 requests, 18154 ok, 0 failed -> 3600.43 req/s
- [PASS] availability under concurrent GET load is >= 99.5%% (evalsheet's own siege -b threshold)
- [info] RSS memory: first-half avg 3956 KB -> second-half avg 3976 KB
- [PASS] RSS memory does not grow unboundedly during the load run (crude leak signal; confirm with valgrind/leaks for a real verdict)
- [PASS] server responds promptly right after the load burst (no pile-up / hanging connections)
- [SKIP] siege stress test  
  siege is not installed (evalsheet: `sudo apt-get install siege`)
- [PASS] server still alive and responsive after the stress run
