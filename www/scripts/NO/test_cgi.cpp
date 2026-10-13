#include <iostream>
#include <string>
#include <cstdlib>

int main() {
    std::cout << "Content-Type: text/html\r\n\r\n";
    std::cout << "<html><body>\n";
    
    const char* method = std::getenv("REQUEST_METHOD");
    const char* query = std::getenv("QUERY_STRING");

    std::cout << "Metodo: " << (method ? method : "NULL") << "<br>\n";
    std::cout << "Query: " << (query ? query : "NULL") << "<br>\n";

    if (method && std::string(method) == "POST") {
        std::cout << "BODY: ";
        char c;
        while (std::cin.get(c)) {
            std::cout << c;
        }
    }
    std::cout << "\n</body></html>\n";
    return 0;
}
