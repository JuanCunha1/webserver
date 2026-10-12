#include <iostream>
#include <string>
#include <vector>
#include <iomanip>

#include "../include/protocol/RequestParser.hpp"
#include "../include/protocol/Request.hpp"

#include "../include/protocol/ResponseBuilder.hpp"
#include "../include/protocol/Response.hpp"

#include "../include/protocol/HttpException.hpp"

#include "../include/protocol/MimeTypes.hpp"

struct TestCase {
    std::string name;
    std::string category;
    std::string rawRequest;
    size_t      maxBodySize;
    int         expectedCode; // HTTP code it must yield (400, 411, 413, 414, 501, 505...)
};

static void runSingleTest(const TestCase &tc, int index, int &passed, int &failed) {
    Request req;
    RequestParser::State state = RequestParser::REQUEST_LINE;
    Response res;

    try {
		std::string rawBuffer = tc.rawRequest;
        // 1. Run the parser with the test request
        RequestParser::process(rawBuffer, req, state, tc.maxBodySize);

        // 2. If parsing succeeds (no exception thrown), move on to the ResponseBuilder
        std::string targetPath = "./www" + req.getUri();
        res = ResponseBuilder::buildResponse(req, targetPath);

    } catch (const HttpException &e) {
        // If the parser threw the exception due to syntax/protocol, build the error response
        res = ResponseBuilder::handleError(req, e.getStatusCode());

        // Also verify that the state machine has been left locked in ERROR
        if (state != RequestParser::ERROR) {
            std::cout << "[\033[33mWARN\033[0m] #" << std::setw(2) << index << " "
                      << tc.name << " -> Lanzó HttpException pero state no quedó en ERROR." << std::endl;
        }
    } catch (const std::exception &e) {
        // Any other unexpected error (bad_alloc, etc.)
        res = ResponseBuilder::handleError(req, 500);
    }

    int actualCode = res.getStatusCode();
    bool ok = (actualCode == tc.expectedCode);

    if (ok) {
        std::cout << "[\033[32mPASS\033[0m] #" << std::setw(2) << index << " " 
                  << std::left << std::setw(42) << tc.name 
                  << " (Got: " << actualCode << ")" << std::endl;
        passed++;
    } else {
        std::cout << "[\033[31mFAIL\033[0m] #" << std::setw(2) << index << " " 
                  << std::left << std::setw(42) << tc.name 
                  << " -> Esperado: \033[33m" << tc.expectedCode 
                  << "\033[0m | Obtenido: \033[31m" << actualCode << "\033[0m" << std::endl;
        failed++;
    }
}

int main() {
    std::vector<TestCase> tests;

    // =========================================================================
    // GROUP 1: REQUEST LINE ERRORS
    // =========================================================================
    {
        TestCase t;
        t.category = "Request Line";
        t.name = "Peticion totalmente vacia";
        t.rawRequest = "";
        t.maxBodySize = 1000;
        t.expectedCode = 400;
        tests.push_back(t);
    }
    {
        TestCase t;
        t.category = "Request Line";
        t.name = "Solo salto de linea CRLF";
        t.rawRequest = "\r\n\r\n";
        t.maxBodySize = 1000;
        t.expectedCode = 400;
        tests.push_back(t);
    }
    {
        TestCase t;
        t.category = "Request Line";
        t.name = "Falta version HTTP";
        t.rawRequest = "GET /index.html\r\nHost: localhost\r\n\r\n";
        t.maxBodySize = 1000;
        t.expectedCode = 400;
        tests.push_back(t);
    }
    {
        TestCase t;
        t.category = "Request Line";
        t.name = "Falta URI (solo metodo y version)";
        t.rawRequest = "GET HTTP/1.1\r\nHost: localhost\r\n\r\n";
        t.maxBodySize = 1000;
        t.expectedCode = 400;
        tests.push_back(t);
    }
    {
        TestCase t;
        t.category = "Request Line";
        t.name = "Elementos de mas en request line";
        t.rawRequest = "GET /index.html HTTP/1.1 EXTRA\r\nHost: localhost\r\n\r\n";
        t.maxBodySize = 1000;
        t.expectedCode = 400;
        tests.push_back(t);
    }
    {
        TestCase t;
        t.category = "Request Line";
        t.name = "Solo LF en vez de CRLF";
        t.rawRequest = "GET /index.html HTTP/1.1\nHost: localhost\n\n";
        t.maxBodySize = 1000;
        t.expectedCode = 400;
        tests.push_back(t);
    }
    {
        TestCase t;
        t.category = "Request Line";
        t.name = "Metodo en minusculas (case sensitive)";
        t.rawRequest = "get /index.html HTTP/1.1\r\nHost: localhost\r\n\r\n";
        t.maxBodySize = 1000;
        t.expectedCode = 400;
        tests.push_back(t);
    }
    {
        TestCase t;
        t.category = "Request Line";
        t.name = "Metodo desconocido inventado";
        t.rawRequest = "CUSTOMMETHOD /index.html HTTP/1.1\r\nHost: localhost\r\n\r\n";
        t.maxBodySize = 1000;
        t.expectedCode = 501;
        tests.push_back(t);
    }
    {
        TestCase t;
        t.category = "Request Line";
        t.name = "Metodo estandar no soportado (PUT)";
        t.rawRequest = "PUT /index.html HTTP/1.1\r\nHost: localhost\r\n\r\n";
        t.maxBodySize = 1000;
        t.expectedCode = 501;
        tests.push_back(t);
    }
    {
        TestCase t;
        t.category = "Request Line";
        t.name = "URI sin slash inicial";
        t.rawRequest = "GET index.html HTTP/1.1\r\nHost: localhost\r\n\r\n";
        t.maxBodySize = 1000;
        t.expectedCode = 400;
        tests.push_back(t);
    }
    {
        TestCase t;
        t.category = "Request Line";
        t.name = "URI excesivamente larga (> 2048 chars)";
        t.rawRequest = "GET /" + std::string(3000, 'a') + " HTTP/1.1\r\nHost: localhost\r\n\r\n";
        t.maxBodySize = 1000;
        t.expectedCode = 414;
        tests.push_back(t);
    }
    {
        TestCase t;
        t.category = "Request Line";
        t.name = "Protocolo no es HTTP (ej. FTP)";
        t.rawRequest = "GET /index.html FTP/1.1\r\nHost: localhost\r\n\r\n";
        t.maxBodySize = 1000;
        t.expectedCode = 400;
        tests.push_back(t);
    }
    {
        TestCase t;
        t.category = "Request Line";
        t.name = "Version no soportada (HTTP/2.0)";
        t.rawRequest = "GET /index.html HTTP/2.0\r\nHost: localhost\r\n\r\n";
        t.maxBodySize = 1000;
        t.expectedCode = 505;
        tests.push_back(t);
    }

    // =========================================================================
    // GROUP 2: HEADER ERRORS
    // =========================================================================
    {
        TestCase t;
        t.category = "Headers";
        t.name = "Falta cabecera obligatoria Host";
        t.rawRequest = "GET /index.html HTTP/1.1\r\nUser-Agent: curl\r\n\r\n";
        t.maxBodySize = 1000;
        t.expectedCode = 400;
        tests.push_back(t);
    }
    {
        TestCase t;
        t.category = "Headers";
        t.name = "Cabecera Host duplicada";
        t.rawRequest = "GET /index.html HTTP/1.1\r\nHost: localhost\r\nHost: test.com\r\n\r\n";
        t.maxBodySize = 1000;
        t.expectedCode = 400;
        tests.push_back(t);
    }
    {
        TestCase t;
        t.category = "Headers";
        t.name = "Cabecera Host con valor vacio";
        t.rawRequest = "GET /index.html HTTP/1.1\r\nHost:\r\n\r\n";
        t.maxBodySize = 1000;
        t.expectedCode = 400;
        tests.push_back(t);
    }
    {
        TestCase t;
        t.category = "Headers";
        t.name = "Espacio previo a dos puntos en header";
        t.rawRequest = "GET /index.html HTTP/1.1\r\nHost : localhost\r\n\r\n";
        t.maxBodySize = 1000;
        t.expectedCode = 400;
        tests.push_back(t);
    }
    {
        TestCase t;
        t.category = "Headers";
        t.name = "Header sin delimitador dos puntos";
        t.rawRequest = "GET /index.html HTTP/1.1\r\nHost localhost\r\n\r\n";
        t.maxBodySize = 1000;
        t.expectedCode = 400;
        tests.push_back(t);
    }
    {
        TestCase t;
        t.category = "Headers";
        t.name = "Caracter de control en el nombre de header";
        t.rawRequest = "GET /index.html HTTP/1.1\r\nHo\x07st: localhost\r\n\r\n";
        t.maxBodySize = 1000;
        t.expectedCode = 400;
        tests.push_back(t);
    }

    // =========================================================================
    // GROUP 3: PAYLOAD AND CONTENT-LENGTH ERRORS
    // =========================================================================
    {
        TestCase t;
        t.category = "Payload";
        t.name = "POST con cuerpo sin Content-Length ni Chunked";
        t.rawRequest = "POST /upload HTTP/1.1\r\nHost: localhost\r\n\r\ndatos_aleatorios";
        t.maxBodySize = 1000;
        t.expectedCode = 411;
        tests.push_back(t);
    }
    {
        TestCase t;
        t.category = "Payload";
        t.name = "Content-Length con valor negativo";
        t.rawRequest = "POST /upload HTTP/1.1\r\nHost: localhost\r\nContent-Length: -10\r\n\r\n";
        t.maxBodySize = 1000;
        t.expectedCode = 400;
        tests.push_back(t);
    }
    {
        TestCase t;
        t.category = "Payload";
        t.name = "Content-Length no numerico";
        t.rawRequest = "POST /upload HTTP/1.1\r\nHost: localhost\r\nContent-Length: diez\r\n\r\n";
        t.maxBodySize = 1000;
        t.expectedCode = 400;
        tests.push_back(t);
    }
    {
        TestCase t;
        t.category = "Payload";
        t.name = "Content-Length duplicado con distinto valor";
        t.rawRequest = "POST /upload HTTP/1.1\r\nHost: localhost\r\nContent-Length: 4\r\nContent-Length: 8\r\n\r\nhola";
        t.maxBodySize = 1000;
        t.expectedCode = 400;
        tests.push_back(t);
    }
    {
        TestCase t;
        t.category = "Payload";
        t.name = "Content-Length supera maxBodySize (413)";
        t.rawRequest = "POST /upload HTTP/1.1\r\nHost: localhost\r\nContent-Length: 100\r\n\r\n" + std::string(100, 'x');
        t.maxBodySize = 20; // Deliberately lower limit
        t.expectedCode = 413;
        tests.push_back(t);
    }

    // =========================================================================
    // GROUP 4: TRANSFER-ENCODING AND HTTP REQUEST SMUGGLING
    // =========================================================================
    {
        TestCase t;
        t.category = "Smuggling & Chunked";
        t.name = "Content-Length y Transfer-Encoding juntos";
        t.rawRequest = "POST /upload HTTP/1.1\r\nHost: localhost\r\nContent-Length: 5\r\nTransfer-Encoding: chunked\r\n\r\n";
        t.maxBodySize = 1000;
        t.expectedCode = 400;
        tests.push_back(t);
    }
    {
        TestCase t;
        t.category = "Smuggling & Chunked";
        t.name = "Transfer-Encoding no soportado (ej. gzip)";
        t.rawRequest = "POST /upload HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: gzip\r\n\r\n";
        t.maxBodySize = 1000;
        t.expectedCode = 501;
        tests.push_back(t);
    }
    {
        TestCase t;
        t.category = "Smuggling & Chunked";
        t.name = "Chunked con tamano hexadecimal invalido";
        t.rawRequest = "POST /upload HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\nXYZ\r\nhola\r\n0\r\n\r\n";
        t.maxBodySize = 1000;
        t.expectedCode = 400;
        tests.push_back(t);
    }

    // =========================================================================
    // EXECUTION
    // =========================================================================
    std::cout << "\n================== TESTER DE PARSEO HTTP (EXCEPCIONES) ==================\n" << std::endl;
    int passed = 0;
    int failed = 0;

    std::string currentCat = "";
    for (size_t i = 0; i < tests.size(); ++i) {
        if (tests[i].category != currentCat) {
            currentCat = tests[i].category;
            std::cout << "\n--- Categoria: " << currentCat << " ---" << std::endl;
        }
        runSingleTest(tests[i], (int)i + 1, passed, failed);
    }

    std::cout << "\n============================== RESUMEN ==============================\n";
    std::cout << "Total Pruebas : " << tests.size() << std::endl;
    std::cout << "Superadas     : \033[32m" << passed << "\033[0m" << std::endl;
    std::cout << "Fallidas      : \033[31m" << failed << "\033[0m" << std::endl;
    std::cout << "=====================================================================\n" << std::endl;

    return (failed == 0 ? 0 : 1);
}