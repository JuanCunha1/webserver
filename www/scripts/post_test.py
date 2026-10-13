#!/usr/bin/env python3
import os
import sys

print("Content-Type: text/html; charset=utf-8")
print("Status: 200 OK")
print()

# 1. Leer la longitud del cuerpo
try:
    content_length = int(os.environ.get("CONTENT_LENGTH", 0))
except ValueError:
    content_length = 0

# 2. Leer los datos enviador por POST desde stdin
body = sys.stdin.read(content_length) if content_length > 0 else "Sin datos"

print("<!DOCTYPE html><html><body>")
print("<h2>Prueba CGI - Método POST</h2>")
print(f"<p><strong>Método recibido:</strong> {os.environ.get('REQUEST_METHOD')}</p>")
print(f"<p><strong>Bytes leídos:</strong> {content_length}</p>")
print(f"<p><strong>Cuerpo procesado:</strong> {body}</p>")
print("</body></html>")