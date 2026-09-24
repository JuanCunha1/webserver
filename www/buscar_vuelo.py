#!/usr/bin/env python3
import os
import urllib.parse
import html

# 1. Cabeceras HTTP obligatorias
print("Content-Type: text/html; charset=utf-8")
print("Status: 200 OK")
print()

# 2. Base de datos simulada
vuelos_db = [
    {"origen": "BCN", "destino": "TIA", "fecha": "2026-10-15", "precio": "85€", "aerolinea": "Albanian Air"},
    {"origen": "BCN", "destino": "BTS", "fecha": "2026-10-15", "precio": "45€", "aerolinea": "Danube Wings"},
    {"origen": "BCN", "destino": "TGD", "fecha": "2026-10-16", "precio": "110€", "aerolinea": "Balkan Express"},
    {"origen": "MAD", "destino": "BCN", "fecha": "2026-10-15", "precio": "30€", "aerolinea": "Iberia Express"}
]

# 3. Extraer y parsear la QUERY_STRING
query_string = os.environ.get("QUERY_STRING", "")
params = urllib.parse.parse_qs(query_string)

# Extraer valores individuales (parse_qs devuelve listas, por eso tomamos el [0])
origen = params.get("origen", [""])[0].upper()
destino = params.get("destino", [""])[0].upper()
fecha = params.get("fecha", [""])[0]

# 4. Construir la respuesta HTML
print("<!DOCTYPE html>")
print("<html lang='es'>")
print("<head><title>Buscador de Vuelos CGI</title></head>")
print("<body style='font-family: sans-serif; margin: 40px;'>")

print("<h2>Buscador de Vuelos</h2>")
print("<form method='GET' action='/buscar.py'>")
print("  Origen: <input type='text' name='origen' placeholder='Ej. BCN' value='" + html.escape(origen) + "'>")
print("  Destino: <input type='text' name='destino' placeholder='Ej. TIA' value='" + html.escape(destino) + "'>")
print("  Fecha: <input type='date' name='fecha' value='" + html.escape(fecha) + "'>")
print("  <button type='submit'>Buscar</button>")
print("</form>")
print("<hr>")

# 5. Lógica de búsqueda
if origen or destino or fecha:
    print("<h3>Resultados de búsqueda:</h3>")
    resultados = []
    
    for vuelo in vuelos_db:
        match_origen = (origen == "" or vuelo["origen"] == origen)
        match_destino = (destino == "" or vuelo["destino"] == destino)
        match_fecha = (fecha == "" or vuelo["fecha"] == fecha)
        
        if match_origen and match_destino and match_fecha:
            resultados.append(vuelo)
            
    if resultados:
        print("<table border='1' cellpadding='8' style='border-collapse: collapse;'>")
        print("<tr><th>Origen</th><th>Destino</th><th>Fecha</th><th>Aerolínea</th><th>Precio</th></tr>")
        for r in resultados:
            print(f"<tr><td>{r['origen']}</td><td>{r['destino']}</td><td>{r['fecha']}</td><td>{r['aerolinea']}</td><td>{r['precio']}</td></tr>")
        print("</table>")
    else:
        print("<p>No se encontraron vuelos para esos criterios.</p>")
else:
    print("<p>Introduce los datos para buscar vuelos.</p>")

print("</body>")
print("</html>")