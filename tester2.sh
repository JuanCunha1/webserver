#!/bin/bash

# ==========================================
# Configuración del Tester
# ==========================================
HOST="http://localhost:8080"
UPLOAD_DIR="/uploads" # Ajusta si tu ruta de subidas es diferente
CGI_SCRIPT="/buscar_vuelo.py"
BIG_FILE="payload_test.tmp"

# Colores
GREEN="\033[0;32m"
RED="\033[0;31m"
YELLOW="\033[0;33m"
BLUE="\033[0;34m"
RESET="\033[0m"

# ==========================================
# Funciones Auxiliares
# ==========================================
print_header() {
    echo -e "\n${BLUE}=========================================${RESET}"
    echo -e "${BLUE}  $1${RESET}"
    echo -e "${BLUE}=========================================${RESET}"
}

check_test() {
    local test_name=$1
    local expected=$2
    local actual=$3

    if [ "$actual" == "$expected" ]; then
        echo -e "${GREEN}[✔] PASS:${RESET} $test_name (HTTP $actual)"
    else
        echo -e "${RED}[✘] FAIL:${RESET} $test_name (Esperado: $expected, Recibido: $actual)"
    fi
}

check_alive() {
    local code=$(curl -s -o /dev/null -w "%{http_code}" $HOST/)
    if [ "$code" == "000" ]; then
        echo -e "${RED}[!] CRASH DETECTADO: El servidor no responde tras la última prueba.${RESET}"
        exit 1
    fi
}

# ==========================================
# 1. Pruebas de Rutas y Errores Básicos
# ==========================================
print_header "1. ENRUTAMIENTO Y ERRORES ESTÁNDARES"

# GET Básico
code=$(curl -s -o /dev/null -w "%{http_code}" $HOST/)
check_test "Página de inicio (GET /)" "200" "$code"

# Archivo Inexistente (404)
code=$(curl -s -o /dev/null -w "%{http_code}" $HOST/archivo_fantasma_42.html)
check_test "Archivo inexistente (404 Not Found)" "404" "$code"

# Método Desconocido (UNKNOWN)
code=$(curl -s -o /dev/null -w "%{http_code}" -X INVENTADO $HOST/)
# Puede devolver 400 Bad Request o 501 Not Implemented, comprobamos que no sea 200 y que no crashee
if [[ "$code" == "400" || "$code" == "501" || "$code" == "405" ]]; then
    echo -e "${GREEN}[✔] PASS:${RESET} Método desconocido (HTTP $code)"
else
    echo -e "${RED}[✘] FAIL:${RESET} Método desconocido (Esperado: 400/405/501, Recibido: $code)"
fi
check_alive

# ==========================================
# 2. Límites y Body
# ==========================================
print_header "2. LÍMITES Y CUERPO DE PETICIÓN"

# Límite de Payload (413)
dd if=/dev/urandom of=$BIG_FILE bs=1M count=2 2>/dev/null
code=$(curl -s -o /dev/null -w "%{http_code}" -X POST --data-binary @$BIG_FILE $HOST/)
check_test "Límite Client Body Size (413 Payload Too Large)" "413" "$code"
rm -f $BIG_FILE
check_alive

# ==========================================
# 3. Métodos POST y DELETE (Uploads)
# ==========================================
print_header "3. UPLOADS (POST Y DELETE)"

# Subir archivo
code=$(curl -s -o /dev/null -w "%{http_code}" -X POST -d "Hola WebServ 42" $HOST$UPLOAD_DIR/eval_test.txt)
if [[ "$code" == "201" || "$code" == "200" ]]; then
    echo -e "${GREEN}[✔] PASS:${RESET} Subida de archivo POST (HTTP $code)"
else
    echo -e "${RED}[✘] FAIL:${RESET} Subida de archivo POST (Esperado: 201/200, Recibido: $code)"
fi

# Borrar archivo
code=$(curl -s -o /dev/null -w "%{http_code}" -X DELETE $HOST$UPLOAD_DIR/eval_test.txt)
if [[ "$code" == "204" || "$code" == "200" || "$code" == "202" ]]; then
    echo -e "${GREEN}[✔] PASS:${RESET} Borrado de archivo DELETE (HTTP $code)"
else
    echo -e "${RED}[✘] FAIL:${RESET} Borrado de archivo DELETE (Esperado: 204/200, Recibido: $code)"
fi

# ==========================================
# 4. Pruebas CGI
# ==========================================
print_header "4. EJECUCIÓN CGI"

# CGI GET con Query String
code=$(curl -s -o /dev/null -w "%{http_code}" "$HOST$CGI_SCRIPT?origen=bcn&destino=tia&fecha=2026-10-15")
check_test "CGI Método GET con Query String" "200" "$code"

# CGI POST con Body (Simulando stdin)
code=$(curl -s -o /dev/null -w "%{http_code}" -X POST -d "param=valor" "$HOST$CGI_SCRIPT")
check_test "CGI Método POST con stdin" "200" "$code"

# CGI Chunked Request
code=$(curl -s -o /dev/null -w "%{http_code}" -X POST -H "Transfer-Encoding: chunked" -d "Contenido Chunked" "$HOST$CGI_SCRIPT")
check_test "CGI Petición Chunked" "200" "$code"

# ==========================================
# 5. Resiliencia de Red (Desconexiones y Pipeling)
# ==========================================
print_header "5. PRUEBAS DE ESTRÉS Y RESILIENCIA (MANUAL)"

echo -e "${YELLOW}Prueba de Desconexión Abrupta (Simulando corte de internet)...${RESET}"
# Se envía la cabecera a medias y se corta el socket cerrando Netcat
(printf "GET / HT"; sleep 1) | nc localhost 8080 > /dev/null 2>&1
check_alive
echo -e "${GREEN}[✔] PASS:${RESET} El servidor sobrevivió a un cliente colgado."

echo -e "\n${BLUE}Para las pruebas de estrés totales, ejecuta manualmente:${RESET}"
echo "1. valgrind --leak-check=full ./webserv config.conf"
echo "2. siege -b -c 50 -t 15s $HOST/"

echo -e "\n${GREEN}=== TESTING FINALIZADO ===${RESET}\n"