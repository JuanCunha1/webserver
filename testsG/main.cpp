#include "../include/protocol/RequestParser.hpp"
#include "../include/protocol/Request.hpp"
#include <iostream>

int main() {
	std::string rawRequest = "GET /index.html HTTP/1.1\r\nContent-Type: Text/Plain\r\nContent-Length: 5\r\nHost: localhost:8080\r\n\r\nasdasdasasdasddasdasdasdasda\r\n";
	//! It would be nice to receive a std::vector<char> for automatic dynamic memory


	size_t maxBodySize = 10000;
	ParsingState state = STATE_REQUEST_LINE;
	Request req;
	
	RequestParser::process(rawRequest, req, state, maxBodySize);
	
	std::cout << "Method: " << req.getMethod() << std::endl;
	std::cout << "URI: " << req.getUri() << std::endl;
	std::cout << "Query: " << req.getQuery() << std::endl;
	std::cout << "Version: " << req.getVersion() << std::endl;
	req.printHeaders();
	//std::cout << "Host: " << *(req.getHeader("host")) << std::endl;
	std::cout << "Body: " << req.getBody() << std::endl;
	
	return (0);
}