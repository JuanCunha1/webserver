#!/usr/bin/env python3
import time
import os

print("Content-Type: text/html\r\n\r\n", end="")

# Simulamos un proceso lento (ej. buscar en BBDD)
time.sleep(3)

print("<!DOCTYPE html><html><body>")
print("<h1>Respuesta tras 3 segundos de espera</h1>")
print(f"<p>Metodo: {os.environ.get('REQUEST_METHOD')}</p>")
print(f"<p>Query: {os.environ.get('QUERY_STRING')}</p>")
print("</body></html>")