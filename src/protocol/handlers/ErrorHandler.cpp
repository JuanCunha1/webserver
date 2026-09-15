#include "../../../include/protocol/ResponseBuilder.hpp"
#include "../../../include/Utils.hpp"
#include <sstream>

static std::string getDefaultStatusMessage(int statusCode) {
    switch (statusCode) {
        case 200: return ("OK");
        case 201: return ("Created");
        case 204: return ("No Content");
        case 400: return ("Bad Request");
        case 403: return ("Forbidden");
        case 404: return ("Not Found");
        case 405: return ("Method Not Allowed");
        case 411: return ("Length Required");
        case 413: return ("Payload Too Large");
        case 414: return ("URI Too Long");
        case 500: return ("Internal Server Error");
        case 501: return ("Not Implemented");
        case 505: return ("HTTP Version Not Supported");
        default:  return ("Unknown Status");
    }
}

Response ResponseBuilder::handleError(const Request &req, int errorCode) {
	Response res;
	std::string statusMsg = getDefaultStatusMessage(errorCode);
	
	res.setStatusCode(errorCode);
	res.setStatusMessage(statusMsg);

	std::ostringstream oss;
	oss << "<html>\r\n"
		<< "<head><title>" << errorCode << " " << statusMsg << "</title></head>\r\n"
		<< "<body style=\"font-family: monospace; text-align: center; margin-top: 50px;\">\r\n"
		<< "<h1>" << errorCode << " " << statusMsg << "</h1>\r\n"
		<< "<hr><p>webserv/1.0</p>\r\n"
		<< "</body>\r\n"
		<< "</html>\r\n";

	std::string body = oss.str();

	res.setHeader("Content-Type", "text/html");
	res.setHeader("Content-Length", Utils::toString(body.size()));

	if (shouldCloseConnection(req, errorCode)) {
		res.setHeader("Connection", "close");
	} else {
		res.setHeader("Connection", "keep-alive");
	}

	res.setBody(body);
	return (res);
}