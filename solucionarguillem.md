Aquí tienes los 8 bugs principales explicados uno por uno: qué falla, cómo reproducirlo tú mismo a mano, y en qué línea exacta del código está la causa. Los primeros 5 los puedes probar con tu propio servidor (./webserv config.conf, puerto 8080). Los últimos 3 (redirect/autoindex/error_page/CGI) tu config.conf actual no los usa, así que uso el config de prueba que ya te dejé montado en webserv_tester/configs/.

1. Config inválida o inexistente → el servidor se cuelga para siempre

cd /home/gfuster/TesteoWebServer
./webserv no_existe.conf
Verás: Error: Cannot open file no_existe.conf, luego Server started with 0 listening socket(s), y nunca vuelve (hay que matarlo con Ctrl+C). Lo mismo pasa con un config con sintaxis rota, un location duplicado, etc.

Causa: src/config/ConfigParser.cpp:33-64 — parseFile() devuelve void y hace return en cualquier error, sin avisar de nada. src/main.cpp:16-23 nunca comprueba si el parseo fue bien; sigue adelante con 0 servidores y llama a server.run(), que se queda esperando en poll() para siempre.

2. Peticiones que el parser rechaza (400/411/413/414/501/505) dejan al cliente colgado

Con tu servidor corriendo (./webserv config.conf), en otra terminal:
printf 'GET /index.html HTTP/1.1\r\n\r\n' | nc -w 3 localhost 8080   # sin Host en HTTP/1.1
printf 'get /index.html HTTP/1.1\r\nHost: localhost\r\n\r\n' | nc -w 3 localhost 8080   # método en minúscula
En ambos casos, no sale nada — nc corta a los 3s sin haber recibido respuesta. Debería devolver 400 Bad Request y cerrar.

Causa: src/network/Server.cpp:293-303. Cuando RequestParser::process() lanza la excepción (400/411/etc.), el catch solo hace std::cerr << "HTTP parse error: ..." y return — nunca construye ni envía ninguna respuesta.

3. Keep-alive roto: la 2ª petición en la misma conexión repite la 1ª

Probar:
(printf "GET /index.html HTTP/1.1\r\nHost: localhost\r\n\r\n"; sleep 1; printf "GET /error.html HTTP/1.1\r\nHost: localhost\r\n\r\n"; sleep 1) | nc localhost 8080

python3 - <<'EOF'
import socket, time
s = socket.create_connection(("127.0.0.1", 8080), timeout=3)
s.sendall(b"GET /index.html HTTP/1.1\r\nHost: localhost\r\n\r\n")
time.sleep(0.2); print("1:", s.recv(4096)[:150])
s.sendall(b"GET /error.html HTTP/1.1\r\nHost: localhost\r\n\r\n")
time.sleep(0.2); print("2:", s.recv(4096)[:150])
EOF
La respuesta "2" debería ser el contenido de error.html, pero te devuelve otra vez el de index.html. Un navegador real reutiliza la conexión, así que esto rompe cualquier página con más de un recurso.

Causa: src/protocol/RequestParser.cpp no tiene ningún método de reset — _state solo se pone a REQUEST_LINE en los constructores (líneas 11 y 17). Tras la primera petición se queda en COMPLETE para siempre. src/network/Server.cpp:305 comprueba if (getState() != COMPLETE) return;, pero como ya está en COMPLETE desde antes, entra directo y reutiliza el Request viejo sin volver a parsear los bytes nuevos.

4. Connection: close se anuncia pero el socket nunca se cierra

python3 - <<'EOF'
import socket, time
s = socket.create_connection(("127.0.0.1", 8080), timeout=3)
s.sendall(b"GET /index.html HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n")
time.sleep(0.3); print(s.recv(4096)[:200])
time.sleep(0.3)
try:
    print("mas datos:", s.recv(16))
except socket.timeout:
    print("BUG: el socket sigue abierto, no llega EOF")
EOF

Causa: src/network/Server.cpp:415-442 (handleClientWrite) nunca mira la cabecera Connection de la respuesta que acaba de enviar; siempre deja el fd en POLLIN (línea 433) como si la conexión siguiera viva.

5. Path traversal: ../ se escapa del www/

curl -i "http://localhost:8080/../Makefile"
Te devuelve el contenido de tu propio Makefile (fuera de www/). Con DELETE sería aún peor, porque podrías borrar archivos fuera del root.

Causa: src/protocol/ResponseBuilder.cpp (buildResponse(), alrededor de la línea 64-83): path = loc.locationRoot + uri sin normalizar ni comprobar que el resultado siga dentro del root.

6. (a b y c) return (redirect), error_page y autoindex se parsean pero no se aplican

Tu config.conf no usa estas directivas, así que para verlo arranca el config de pruebas que ya dejé listo:
cd /home/gfuster/TesteoWebServer/webserv_tester
../webserv configs/basic.conf &
curl -i http://127.0.0.1:8500/redirect/        # esperarías 302 + Location, sale 200 normal
curl -i http://127.0.0.1:8500/no-existe-esto   # error_page 404 apunta a una página custom, sale la genérica
curl -i http://127.0.0.1:8500/autoindex-on/    # autoindex on, sale 403 en vez de listado
kill %1

Causas:
- redirect: src/core/Router.cpp está vacío (0 líneas). Nada en el proyecto lee loc.returnRedirections fuera del parser (grep -rn returnRedirections src solo aparece en config/*.cpp).
- error_page: src/protocol/handlers/ErrorHandler.cpp (handleError()) construye siempre su propio HTML genérico; server.errorPages no se lee en ningún sitio fuera del parser.
- autoindex: src/protocol/handlers/GetHandler.cpp, líneas ~70-74 — la llamada generateAutoindex(...) está comentada, así que siempre cae a 403.

7. El CGI no se ejecuta en su propio directorio (falta chdir)

cd /home/gfuster/TesteoWebServer/webserv_tester
../webserv configs/cgi.conf &
curl http://127.0.0.1:8520/reldata.py
kill %1
Este script intenta abrir reldata.txt (que está justo al lado, en www/cgi-bin/) por ruta relativa. Debería devolver FOUND:marker:RELDATA_OK, pero da NOTFOUND porque el CGI hereda el directorio de trabajo desde donde lanzaste webserv, no la carpeta del script.

Causa: src/protocol/handlers/CgiHandler.cpp, función executeChild() (líneas 101-120): nunca llama a chdir() antes del execve().

8. CONTENT_TYPE nunca llega al CGI en un POST

curl -X POST -H "Content-Type: text/plain" --data "hola" http://127.0.0.1:8520/env.py
En la salida busca la línea CONTENT_TYPE= — sale <MISSING> en vez de text/plain.

Causa: src/protocol/handlers/CgiHandler.cpp:342 hace req.getHeader("Content-Type") (con mayúsculas), pero src/protocol/RequestParser.cpp guarda todas las cabeceras en minúsculas (línea ~212, tolower). Como Request::getHeader usa std::map::find con comparación exacta, la búsqueda nunca coincide.

---

Estos 8 son los más graves, pero el informe completo tiene 46 fallos en total (headers duplicados, chunked mal formado, límite de body, etc.) — todos con el mismo tipo de explicación + causa. Los tienes en webserv_tester/README.md y en webserv_tester/logs/last_run_report.md, o relanzando python3 run_tests.py desde webserv_tester/.