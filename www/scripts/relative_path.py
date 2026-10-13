#!/usr/bin/env python3
import os

print("Content-Type: text/html; charset=utf-8")
print("Status: 200 OK")
print()

print("<!DOCTYPE html><html><body>")
print(f"<p><strong>Directorio de trabajo actual (CWD):</strong> {os.getcwd()}</p>")

try:
    # Intenta abrir el archivo mediante una RUTA RELATIVA
    with open("./datos.txt", "r") as f:
        contenido = f.read()
    print(f"<p><strong>Contenido leído:</strong> {contenido}</p>")
except Exception as e:
    print(f"<p style='color:red;'><strong>Error abriendo ruta relativa:</strong> {e}</p>")

print("</body></html>")