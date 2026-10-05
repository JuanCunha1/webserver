#!/usr/bin/env python3
import sys
import os
import time

# 1. Cabeceras HTTP obligatorias
print("Content-Type: text/html; charset=utf-8")
print("Status: 200 OK")
print()

print("<!DOCTYPE html><html><body style='font-family: sans-serif; margin: 40px;'>")
print("<h2>Test de POST CGI: Lectura por stdin</h2>")

# 2. Comprobar la variable de entorno
content_length_env = os.environ.get("CONTENT_LENGTH", "0")
print(f"<p><strong>CONTENT_LENGTH recibido:</strong> {content_length_env} bytes</p>")

try:
    content_length = int(content_length_env)
except ValueError:
    content_length = 0

# 3. Leer el body progresivamente
chunk_size = 16384 # 16 KB por lectura
bytes_read = 0
start_time = time.time()

print("<h3>Progreso de lectura:</h3><ul>")

# Simulamos un intérprete lento para forzar que el pipe del webserv se llene (EAGAIN)
while bytes_read < content_length:
    chunk = sys.stdin.read(min(chunk_size, content_length - bytes_read))
    if not chunk:
        break
    
    bytes_read += len(chunk)
    print(f"<li>Leídos {len(chunk)} bytes... (Total: {bytes_read})</li>")
    
    # Pausa de 50ms para que el pipe del lado de C++ se llene y devuelva EAGAIN
    time.sleep(0.05) 

end_time = time.time()

print("</ul>")
print("<hr>")
print(f"<h3>Resultado:</h3>")
print(f"<p>Total de bytes leídos por stdin: <strong>{bytes_read}</strong></p>")
print(f"<p>Tiempo total: <strong>{end_time - start_time:.2f} segundos</strong></p>")

if bytes_read == content_length and content_length > 0:
    print("<p style='color: green;'><strong>¡ÉXITO!</strong> Todos los bytes se transmitieron correctamente.</p>")
else:
    print("<p style='color: red;'><strong>ERROR:</strong> Discrepancia entre CONTENT_LENGTH y los bytes reales leídos.</p>")

print("</body></html>")