#include "../include/protocol/RequestParser.hpp"
#include "../include/protocol/Request.hpp"

#include "../include/protocol/ResponseBuilder.hpp"
#include "../include/protocol/Response.hpp"

#include "../include/protocol/HttpException.hpp"

#include "../include/protocol/MimeTypes.hpp"
#include <iostream>

int main() {

	Request			req;
    HandlerResult	res;

	try {
		//* --- REQUEST ---
		std::string rawRequest = 
        	"GET /buscar_vuelo.py?origen=bcn&destino=tia&fecha=2026-10-15 HTTP/1.1\r\n"
        	//"Content-Type: application/x-www-form-urlencoded\r\n"
        	//"Content-Length: 17\r\n"
        	"Host: localhost:8080\r\n"
        	"\r\n";
        	//"nombre=estudiante";

		//! Estaria bien recibir de un std::vector<char> para memoria dinamica automatica

		size_t maxBodySize = 10000;
		RequestParser::State state = RequestParser::REQUEST_LINE;
		
		RequestParser::process(rawRequest, req, state, maxBodySize);

		std::cout << "--- REQUEST ---" << std::endl;
		std::cout << "Method: " << req.getMethod() << std::endl;
		std::cout << "URI: " << req.getUri() << std::endl;
		std::cout << "Query: " << req.getQuery() << std::endl;
		std::cout << "Version: " << req.getVersion() << std::endl;
		req.printHeaders();
		std::cout << "Body: " << req.getBody() << std::endl;
		std::cout << std::endl;

		//* --- CONFIG ---
		ConfigLocation loc;
		loc.allowedMethods.push_back("GET");
		loc.allowedMethods.push_back("POST");
		loc.cgiExtension.push_back(".py"); 
		loc.cgiPath.push_back("/usr/bin/python3");
		
		
		// --- RESPONSE ---
		//! La ruta que le pasamos es la que recibo del conf (ahora es provisional)
		std::string targetPath = "./www" + req.getUri();

		/*
		res = ResponseBuilder::buildResponse(req, targetPath, loc);

		if (res.isCgi == false) {
			std::cout << "--- SERIALIZE ---" << std::endl;
			//Se cambia por Serialize Real haciendo el send
			//res.setStatusCode(0);
			std::cout << "Serialize: " << res.staticResponse.getHeadersAsString() << res.staticResponse.getBody() <<std::endl;
		}

			
		} catch (const HttpException &e) {
			res.staticResponse = ResponseBuilder::handleError(req, e.getStatusCode());
			//Se cambia por Serialize Real haciendo el send
			std::cout << "Serialize: " << res.staticResponse.getHeadersAsString() << res.staticResponse.getBody() <<std::endl;
			//send(client_fd, errHeaders.data(), errHeaders.size(), 0);
			//send(client_fd, errorRes.getBody().data(), errorRes.getBody().size(), 0);
		}
		*/
	//* --- EJECUCIÓN DEL HANDLER ---
		res = ResponseBuilder::buildResponse(req, targetPath, loc);

		if (res.isCgi == false) {
			std::cout << "--- RESPUESTA ESTÁTICA ---" << std::endl;
			std::cout << res.staticResponse.getHeadersAsString() << res.staticResponse.getBody() << std::endl;
		} 
		else {
			std::cout << "--- CGI DETECTADO: INICIANDO BUCLE DE PRUEBA ---" << std::endl;
				
			// Simulación del multiplexor: forzamos las llamadas hasta que termine
			while (!res.cgiHandler->isReadDone() && !res.cgiHandler->hasError()) {
					
				// Si el CGI espera datos en STDIN (ej. un POST)
				if (!res.cgiHandler->isWriteDone()) {
					res.cgiHandler->writeToCgi();
				}
					
				// Leemos del STDOUT del CGI
				res.cgiHandler->readFromCgi();
			}

			if (res.cgiHandler->hasError()) {
				std::cout << "ERROR: El CGI ha fallado durante la ejecución." << std::endl;
			} else {
				std::cout << "--- CGI COMPLETADO: CONSTRUYENDO RESPUESTA ---" << std::endl;
				Response cgiRes = res.cgiHandler->buildCgiResponse();
					
				std::cout << "Status code: " << cgiRes.getStatusCode() << std::endl;
				std::cout << "Serialize:\n" << cgiRes.getHeadersAsString() << cgiRes.getBody() << std::endl;
			}

			// Limpieza manual de la memoria
			delete res.cgiHandler;
			res.cgiHandler = NULL;
		}
	}
	catch (const HttpException &e) { // Asumiendo que tienes esta excepción
        res.staticResponse = ResponseBuilder::handleError(req, e.getStatusCode());
        std::cout << "--- ERROR SERIALIZE ---\n" << res.staticResponse.getHeadersAsString() << res.staticResponse.getBody() << std::endl;
    }

	/*
	std::cout << "--- RESPONSE ---" << std::endl;
	std::cout << "Version: " << res.staticResponse.getVersion() << std::endl;
	std::cout << "Status code: " << res.staticResponse.getStatusCode() << std::endl;
	std::cout << "Status Message: " << res.staticResponse.getStatusMessage() << std::endl;
	res.staticResponse.printHeadersRes();
	std::cout << "Body: " << res.staticResponse.getBody() << std::endl;

	//std::cout << "--- SERIALIZE ---" << std::endl;
	//std::cout << "Serialize: " << res.getHeadersAsString() << res.getBody() <<std::endl;
	*/

	/*
	std::cout << "--- CHECK INVISIBLE CHARACTERS ---" << std::endl;
	std::string raw = res.getHeadersAsString();
	for (size_t i = 0; i < raw.size(); ++i) {
		if (raw[i] == '\r') std::cout << "\\r";
		else if (raw[i] == '\n') std::cout << "\\n\n";
		else std::cout << raw[i];
	}
	*/
	return (0);
}