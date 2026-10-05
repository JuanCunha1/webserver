#include <iostream>
#include <string>
#include <cstdlib> // Para getenv

int main() {
    // 1. OBLIGATORIO: Todo CGI debe devolver primero sus cabeceras HTTP
    // seguidas de dos saltos de línea (\r\n\r\n) para separar del body.
    std::cout << "Content-Type: text/html\r\n\r\n";

    std::cout << "<html><head><title>Test CGI C++</title></head><body>\n";
    std::cout << "<h1>¡Funciona! CGI Compilado ejecutado con exito</h1>\n";

    // 2. Leer variables de entorno que tu 'buildEnv' configuró
    const char* method = std::getenv("REQUEST_METHOD");
    const char* queryString = std::getenv("QUERY_STRING");

    std::cout << "<h2>Datos del Request:</h2>\n";
    std::cout << "<ul>\n";
    std::cout << "<li><b>Metodo:</b> " << (method ? method : "NO DEFINIDO") << "</li>\n";
    std::cout << "<li><b>Query String:</b> " << (queryString ? queryString : "VACIO") << "</li>\n";
    std::cout << "</ul>\n";

    // 3. Si es POST, el body debe leerse desde STDIN
    if (method && std::string(method) == "POST") {
        std::cout << "<h2>Cuerpo del POST recibido:</h2>\n";
        std::cout << "<pre style=\"background:#eee; padding:10px;\">\n";
        
        char c;
        // Leemos byte a byte desde STDIN hasta que se acabe el pipe
        while (std::cin.get(c)) {
            std::cout << c;
        }
        
        std::cout << "</pre>\n";
    }

    std::cout << "</body></html>\n";
    return 0;
}