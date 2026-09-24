/*
#include "../include/protocol/RequestParser.hpp"
#include "../include/protocol/Request.hpp"

#include "../include/protocol/ResponseBuilder.hpp"
#include "../include/protocol/Response.hpp"

#include "../include/protocol/HttpException.hpp"

#include "../include/protocol/MimeTypes.hpp"
#include <iostream>
*/
#include "protocol/RequestParser.hpp"
#include "protocol/ResponseBuilder.hpp"
#include <iostream>
#include <string>
#include <unistd.h> // Para usleep()

int main() {
    Request         req;
    HandlerResult   res;

    try {
        //* --- 1. MOCK DE LA REQUEST ---
        // Petición GET con parámetros en la URL (Query String)
        std::string rawRequest = 
            "GET /lento.py?id=123&usuario=estudiante HTTP/1.1\r\n"
            "Host: localhost:8080\r\n"
            "\r\n";

        size_t maxBodySize = 10000;
        RequestParser::State state = RequestParser::REQUEST_LINE;
        
        // Parseamos la string bruta hacia tu clase Request
        RequestParser::process(rawRequest, req, state, maxBodySize);

        std::cout << "--- REQUEST PARSEADA ---" << std::endl;
        std::cout << "Method: " << req.getMethod() << std::endl;
        std::cout << "URI: " << req.getUri() << std::endl;
        std::cout << "Query: " << req.getQuery() << std::endl;
        std::cout << "Version: " << req.getVersion() << std::endl;
        std::cout << std::endl;

        //* --- 2. MOCK DE LA CONFIGURACIÓN (Location) ---
        ConfigLocation loc;
        loc.allowedMethods.push_back("GET");
		loc.allowedMethods.push_back("POST");
		loc.cgiExtension.push_back(".py"); 
		loc.cgiPath.push_back("/usr/bin/python3");

        std::string targetPath = "./www" + req.getUri();

        //* --- 3. CONSTRUCCIÓN DE LA RESPUESTA ---
        res = ResponseBuilder::buildResponse(req, targetPath, loc);

        if (res.isCgi == false) {
            //* CASO A: RESPUESTA ESTÁTICA
            std::cout << "--- RESPUESTA ESTÁTICA ---" << std::endl;
            std::cout << res.staticResponse.getHeadersAsString() << res.staticResponse.getBody() << std::endl;
        } 
        else {
            //* CASO B: CGI DETECTADO (SIMULACIÓN DEL EVENT LOOP)
            std::cout << "--- CGI DETECTADO: INICIANDO BUCLE ASÍNCRONO ---" << std::endl;
            
            int ciclosDelEventLoop = 0;

            // Este while simula el poll() o epoll() infinito del Integrante 1
            while (!res.cgiHandler->isReadDone() && !res.cgiHandler->hasError()) {
                
                ciclosDelEventLoop++;
                
                // Imprimimos algo cada 5 ciclos para demostrar que el hilo principal no está bloqueado
                if (ciclosDelEventLoop % 5 == 0) {
                    std::cout << "[Event Loop] Sirviendo archivos a otros clientes... (Tick " << ciclosDelEventLoop << ")" << std::endl;
                }

                // Si la petición fuera POST, el event loop llamaría a writeToCgi()
                if (req.getMethod() == "POST" && !res.cgiHandler->isWriteDone()) {
                    res.cgiHandler->writeToCgi();
                }
                
                // Intentamos leer. Como ahora los pipes tienen O_NONBLOCK, esto retorna al instante si no hay datos.
                res.cgiHandler->readFromCgi();
                
                // Pausamos 100 milisegundos reales para no freír la CPU en esta simulación
                usleep(100000); 
            }

            if (res.cgiHandler->hasError()) {
                std::cout << "\nERROR: El proceso CGI ha fallado." << std::endl;
            } else {
                std::cout << "\n--- CGI COMPLETADO: GENERANDO HTTP ---" << std::endl;
                Response cgiRes = res.cgiHandler->buildCgiResponse();
                
                std::cout << "Status code: " << cgiRes.getStatusCode() << std::endl;
                std::cout << "\nSerialize:\n" << cgiRes.getHeadersAsString() << cgiRes.getBody() << std::endl;
            }

            // Limpiamos la memoria dinámica del CGI (vital para no tener leaks)
            delete res.cgiHandler;
            res.cgiHandler = NULL;
        }
        
    } catch (const std::exception &e) { // Captura tu HttpException genérica si la tienes
        // Aquí asumimos que tienes e.getStatusCode(), ajústalo a tu código
        // res.staticResponse = ResponseBuilder::handleError(req, e.getStatusCode());
        std::cout << "--- ERROR CATCH ---\nExcepción capturada." << std::endl;
    }

    return (0);
}