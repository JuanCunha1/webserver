#include "../include/protocol/RequestParser.hpp"
#include "../include/protocol/Request.hpp"

#include "../include/protocol/ResponseBuilder.hpp"
#include "../include/protocol/Response.hpp"

#include "../include/protocol/HttpException.hpp"

#include "../include/protocol/MimeTypes.hpp"
#include <iostream>

int main() {

	Request  req;
    Response res;

	try {
	//* --- REQUEST ---
	std::string rawRequest = "GET /index.html HTTP/1.1\r\nContent-Type: Text/Plain\r\nContent-Length: 5\r\nHost: localhost:8080\r\n\r\n";
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

	//* --- RESPONSE ---
	//! La ruta que le pasamos es la que recibo del conf (ahora es provisional)
	std::string targetPath = "./www" + req.getUri();
	res = ResponseBuilder::buildResponse(req, targetPath);

	std::cout << "--- SERIALIZE ---" << std::endl;
	//*Se cambia por Serialize Real haciendo el send
	//res.setStatusCode(0);
	std::cout << "Serialize: " << res.getHeadersAsString() << res.getBody() <<std::endl;
	/*
	std::string headers = res.getHeadersAsString();
	send(client_fd, headers.data(), headers.size(), 0);
	if (!res.getBody().empty()) {
		send(client_fd, res.getBody().data(), response.getBody().size(), 0);
	}
	*/
		
	} catch (const HttpException &e) {
		res = ResponseBuilder::handleError(req, e.getStatusCode());
		//*Se cambia por Serialize Real haciendo el send
		std::cout << "Serialize: " << res.getHeadersAsString() << res.getBody() <<std::endl;
		//send(client_fd, errHeaders.data(), errHeaders.size(), 0);
    	//send(client_fd, errorRes.getBody().data(), errorRes.getBody().size(), 0);
	}

	
	std::cout << "--- RESPONSE ---" << std::endl;
	std::cout << "Version: " << res.getVersion() << std::endl;
	std::cout << "Status code: " << res.getStatusCode() << std::endl;
	std::cout << "Status Message: " << res.getStatusMessage() << std::endl;
	res.printHeadersRes();
	std::cout << "Body: " << res.getBody() << std::endl;

	//std::cout << "--- SERIALIZE ---" << std::endl;
	//std::cout << "Serialize: " << res.getHeadersAsString() << res.getBody() <<std::endl;
	
	std::cout << "--- CHECK INVISIBLE CHARACTERS ---" << std::endl;
	std::string raw = res.getHeadersAsString();
	for (size_t i = 0; i < raw.size(); ++i) {
		if (raw[i] == '\r') std::cout << "\\r";
		else if (raw[i] == '\n') std::cout << "\\n\n";
		else std::cout << raw[i];
	}
	return (0);
}