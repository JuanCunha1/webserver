#!/usr/bin/env python3
import sys
import os

# 1. Output de Headers
# Es vital imprimir el header seguido de una línea en blanco.
# Tu método splitOutput() busca "\r\n\r\n" o "\n\n" para separar esto del body.
print("Content-Type: text/html")
print("Status: 200 OK")
print() # Imprime el salto de línea vacío que delimita los headers

# 2. Output del Body
print("<!DOCTYPE html>")
print("<html>")
print("<head><title>Prueba CGI</title></head>")
print("<body>")
print("<h1>CGI Ejecutado Correctamente</h1>")

# Leer variables de entorno generadas por tu método buildEnv()
method = os.environ.get("REQUEST_METHOD", "Desconocido")
print("<h3>Información de la Petición</h3>")
print("<ul>")
print("<li><strong>Método:</strong> " + method + "</li>")
print("<li><strong>URI:</strong> " + os.environ.get("SCRIPT_NAME", "N/A") + "</li>")
print("<li><strong>Query String:</strong> " + os.environ.get("QUERY_STRING", "N/A") + "</li>")
print("</ul>")

# 3. Leer el STDIN si es un POST (lo que tu servidor envía con writeToCgi)
if method == "POST":
    try:
        content_length = int(os.environ.get("CONTENT_LENGTH", "0"))
    except ValueError:
        content_length = 0

    if content_length > 0:
        # Lee exactamente los bytes indicados por Content-Length desde el pipe de entrada
        body = sys.stdin.read(content_length)
        print("<h3>Body Recibido (vía Pipe STDIN):</h3>")
        print("<pre>" + body + "</pre>")
    else:
        print("<p>Petición POST recibida sin Content-Length o con tamaño 0.</p>")

print("</body>")
print("</html>")