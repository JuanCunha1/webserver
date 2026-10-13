#!/usr/bin/env python3
import os

print("Content-Type: text/plain\r\n\r\n", end="")
content_type = os.environ.get("CONTENT_TYPE", "<MISSING>")
print(f"CONTENT_TYPE={content_type}")

# Opcional: descomentar para volcar todo el entorno
# for key, val in os.environ.items():
#     print(f"{key}={val}")