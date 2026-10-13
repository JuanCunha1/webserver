import sys

print("Content-Type: text/plain\r\n\r")
try:
    # Intenta abrir el archivo por ruta relativa (sin ruta absoluta)
    with open("reldata.txt", "r") as file:
        print(f"FOUND:marker:{file.read()}")
except Exception as e:
    print(f"NOTFOUND: {e}")
