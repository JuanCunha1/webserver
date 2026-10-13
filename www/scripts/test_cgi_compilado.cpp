#include <iostream>
#include <string>
#include <cstdlib> // For getenv

int main() {
    // 1. MANDATORY: Every CGI must first return its HTTP headers
    // followed by two line breaks (\r\n\r\n) to separate from the body.
    std::cout << "Content-Type: text/html\r\n\r\n";

    std::cout << "<html><head><title>Test CGI C++</title></head><body>\n";
    std::cout << "<h1>¡Funciona! CGI Compilado ejecutado con exito</h1>\n";

    // 2. Read environment variables that your 'buildEnv' set
    const char* method = std::getenv("REQUEST_METHOD");
    const char* queryString = std::getenv("QUERY_STRING");

    std::cout << "<h2>Datos del Request:</h2>\n";
    std::cout << "<ul>\n";
    std::cout << "<li><b>Metodo:</b> " << (method ? method : "NO DEFINIDO") << "</li>\n";
    std::cout << "<li><b>Query String:</b> " << (queryString ? queryString : "VACIO") << "</li>\n";
    std::cout << "</ul>\n";

    // 3. If it is POST, the body must be read from STDIN
    if (method && std::string(method) == "POST") {
        std::cout << "<h2>Cuerpo del POST recibido:</h2>\n";
        std::cout << "<pre style=\"background:#eee; padding:10px;\">\n";
        
        char c;
        // Read byte by byte from STDIN until the pipe ends
        while (std::cin.get(c)) {
            std::cout << c;
        }
        
        std::cout << "</pre>\n";
    }

    std::cout << "</body></html>\n";
    return 0;
}