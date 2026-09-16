#include "../../include/protocol/ResponseBuilder.hpp"
#include "../../include/protocol/MimeTypes.hpp"
#include "../../include/Utils.hpp"

#include <fstream>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

//! Funciones que debere hacer
//isCgiRequest();
//handleCgi();

Response ResponseBuilder::buildErrorResponse(int code, const std::string &msg) {
	Response res;
	std::string defaultBody = "<html><body><h1>" + Utils::toString(code) + " " + msg + "</h1></body></html>";

	res.setStatusCode(code);
	res.setStatusMessage(msg);
	res.setHeader("Content-Type", "text/html");
	res.setHeader("Content-Length", Utils::toString(defaultBody.size()));
	res.setHeader("Connection", "close");
	res.setBody(defaultBody);

	return (res);
}

bool ResponseBuilder::shouldCloseConnection(const Request &req, int statusCode) {
	const std::string* conn = req.getHeader("Connection");
	if (conn && *conn == "close") {
		return (true);
	}
	if (req.getVersion() == "HTTP/1.0" && !(conn != NULL && *conn == "keep-alive")) {
		return (true);
	}
	if (statusCode == 400 || statusCode == 413 || statusCode == 500 || statusCode == 501) {
		return (true);
	}
	return (false);
}

Response ResponseBuilder::buildResponse(const Request &req, const std::string &path) {
	//* Comprobamos que metodo es, si no es ninguno codigo de error...
	const std::string& method = req.getMethod();

	if (method == "GET") {
		return (handleGet(req, path));
	} else if (method == "POST") {
		return (handlePost(req, path));
	} else if (method == "DELETE") {
		return (handleDelete(req, path));
	}  else {
		return (handleError(req, 501));
	}
}