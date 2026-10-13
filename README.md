_This project has been created as part of the 42 curriculum by gfuster, aiguerre_



# 💬 **Description**

The goal of the project is to write an HTTP server in C++98. The server must be fully compatible with a web browser, handling HTTP/1.1 requests and serving static websites. The project relies heavily on multiplexing to handle multiple clients simultaneously without crashing or hanging.

According to the subject, the server must feature:
- Non-blocking I/O operations using `poll()` (or equivalents like `epoll`/`kqueue`/`select`).
- Configuration files inspired by NGINX syntax to manage ports, hosts, and routing.
- Default error pages and handling of HTTP status codes.
- Directory listing (`autoindex`).
- HTTP Redirections (e.g., 301, 302).
- Route-specific constraints (e.g., restricting HTTP methods like GET, POST, DELETE).
- File uploads via POST requests.
- CGI execution for dynamic content based on file extensions (e.g., `.php` or `.py`).
- Strict memory management with no memory leaks.

Overall, this web server proves how HTTP communication works under the hood, managing raw TCP sockets and parsing HTTP requests from scratch.

### ⚙️ Core System Calls used

To ensure full compliance with the subject and proper non-blocking I/O behavior, we relied on the following key system calls:

- **`accept()`**: Called when a listening socket receives a new connection to generate a dedicated client socket.
- **`recv()`**: Used to read incoming HTTP requests from the client's socket into our internal buffers.
- **`send()`**: Used to transmit the generated HTTP responses (headers and body) back to the client over the network.
- **`write()` / `read()`**: Used strictly for local file operations, such as reading an `index.html` from disk, saving a file uploaded via POST, or communicating with CGI scripts through pipes.


# 📝 **Instructions**

First of all, you must compile the project using the terminal inside the root directory.

```bash
git clone [repository_url]
cd webserv
make
```

You can also use the following:

```
make clean
make fclean
make re
```

Once that is done, you can launch the server by providing a configuration file.

```bash
./webserv [path/to/config.conf]
```

From now on, the server will be listening on the ports specified in your `.conf` file. You can test it by opening a browser (e.g., Chrome or Firefox) and navigating to `http://localhost:[port]` or using tools like `curl`:

```bash
curl -v http://localhost:8080
```

To gracefully close the server, just press `Ctrl + C` in the terminal.

Note: in this case it is important to not use https instead of http due to privacy issues we do not have to manage.

# 🔎 **Resources**

- [HTTP/1.1 RFC 2616](https://datatracker.ietf.org/doc/html/rfc2616)
- [Beej's Guide to Network Programming](https://beej.us/guide/bgnet/)
- [NGINX documentation](https://nginx.org/en/docs/)
- [MDN Web Docs: HTTP Headers](https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers)
- [CGI (Common Gateway Interface) specifications](https://datatracker.ietf.org/doc/html/rfc3875)


## How AI was used:

We used AI to organize ourselves in the beggining of the project. At first we didn't knew how to distribute equally the tasks so we checked with AI, besides other people who had done the project before. Furthermore we used it to manage better the code architechture and to upgrade the code, once we had the base comprehended.


# 🧠 **Technical decisions**

**How did we realize the project?**

The architecture is divided into three main blocks: Configuration Parsing, Server Management (Multiplexing), and HTTP Processing.

First, the program parses the `.conf` file. It extracts the virtual servers, ports, error pages, and location blocks, storing them in C++ data structures. We made sure to implement strict validation: if multiple server blocks attempt to bind to the same host/port without proper Virtual Hosting setup, the program safely catches the `bind()` failure and terminates without crashing.

Secondly, the core execution starts. We create a listening socket for each configured port, bind them, and set them to non-blocking mode (`fcntl(O_NONBLOCK)`). We decided to use `poll()` to monitor all file descriptors. This allows a single thread to multiplex I/O operations, meaning the server can accept new connections, read incoming requests, and send responses simultaneously without blocking other clients.

Inside the main server loop, `poll()` monitors all sockets at the same time and triggers specific events:
- If `poll()` flags a **listening socket**, it means there is a new incoming connection. We call `accept()` to create a new client socket, set it to non-blocking, and add it to our `poll()` tracking list.
- If it flags a **client socket with `POLLIN`**, data is ready to be read. We call `recv()` to read the raw HTTP request into a buffer. Our HTTP parser then breaks it down to extract the method (GET, POST, DELETE), the requested URI, and the headers.
- If it flags a **client socket with `POLLOUT`**, the server is ready to write. We call `send()` to deliver the generated HTTP response back to the browser.

Finally, the Response generation part takes over:
- If the method is `GET` and it's a file, we serve it with the correct `MIME-type` and calculate the `Content-Length`.
- If the URI is a directory and `autoindex` is ON, we dynamically generate an HTML listing using `opendir()` and `readdir()`.
- If the route specifies a CGI (e.g., Python), we use `fork()`, set up pipes to communicate, and use `execve()` to run the script, capturing its output to send back to the client.
- If the resource is not found or a method is not allowed, we construct the proper HTTP error response (e.g., 404, 405) and send it.

Once the response is fully sent using `send()`, the connection is either closed or kept alive depending on the headers, keeping the main loop running seamlessly.


# 🧪 **Evaluation Tests & Commands**

**Set up multiple websites on different interfaces and ports.**
config1.conf
http://127.0.0.1:8080/index.html
http://127.0.0.1:8081/index.html
http://127.0.0.2:8080/index.html

curl http://127.0.0.1:8080
curl http://127.0.0.1:8081
curl http://127.0.0.2:8080

**Set up a default error page (try modifying the 404 error page).**
config2.conf
http://localhost:8080/asdasd

curl http://localhost:8080/asdasd

**Limit the size of the client request body (use: curl -X POST -H "Content-Type: plain/text" --data "BODY IS HERE write something shorter or longer than body limit").**
config2.conf
curl -X POST -H "Content-Type: plain/text" --data "BODY" http://localhost:8080/prueba.txt
-v (antes del -X para ver cuerpo)

**Set up routes on a server to different directories.**
config2.conf
curl -v http://127.0.0.1:8080/
curl -v http://127.0.0.1:8080/camisetas/camiseta_roja.html

**Set up a default file to serve when requesting a directory**
config2.conf
curl -v http://127.0.0.1:8080/camisetas/

**Set up a list of accepted methods for a specific route (e.g., try DELETE something with and without permission).**
config2.conf
Con nc o…
curl -v -X DELETE http://127.0.0.1:8080/camisetas/camiseta_roja.html


**CHECK CGI**
curl “http://127.0.0.1:8080/buscar_vuelo.py?origen=BCN”

**Script para probar Infinite (infinite.py):**
curl http://127.0.0.1:8080/infinite.py

**Script para probar Lento (lento.py):**
curl http://127.0.0.1:8080/lento.py

**Script para probar POST (post_test.py):**
curl -v -X POST -H "Content-Type: application/x-www-form-urlencoded" --data "usuario=guillem&proyecto=webserv" http://127.0.0.1:8080/post_test.py

**Script para probar Rutas Relativas (relative_path.py):**
curl -v http://127.0.0.1:8080/relative_path.py

**Script con Error de Ejecución (error_crash.py):**
curl -v http://127.0.0.1:8080/error_crash.py

**PRUEBAS PARA CREAR WEB**
mkdir -p www/prueba
echo "<h1>Prueba superada</h1>" > www/prueba/index.html


**Using telnet, curl, and pre-prepared files, demonstrate that the**
**following features function correctly:**

**GET, POST, and DELETE requests should work.**
**UNKNOWN requests should not result in a crash.**
For every test, you should receive the appropriate status code.
Upload some files to the server and retrieve them.

**Pruebas GET:**
Con nc manual…

**Pruebas POST i DELETE:**
echo "Contenido de prueba para el servidor" > test.txt
curl -v -X POST --data-binary "@test.txt" -H "Content-Type: text/plain" http://127.0.0.1:8080/uploads/test.txt
curl -v http://127.0.0.1:8080/uploads/test.txt
curl -v -X DELETE http://127.0.0.1:8080/uploads/test.txt
curl -v http://127.0.0.1:8080/uploads/test.txt

**Subir imagenes:**
curl -v -X POST --data-binary "@www/tests/imagen.jpg" -H "Content-Type: image/jpeg" http://127.0.0.1:8080/uploads/image.jpg
Descargar en local:
curl -v http://127.0.0.1:8080/uploads/image.jpg -o descarga.jpg

**SIEGE & STRESS TEST**
siege -b -c 100 -t 20s http://127.0.0.1:8080/empty.html

**Antes/durante/despues (para medir fds):**
lsof -p $(pgrep webserv) | wc -l


