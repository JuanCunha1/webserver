#include "protocol/ResponseBuilder.hpp"
#include "protocol/MimeTypes.hpp"
#include "Utils.hpp"

#include <fstream>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

ResponseBuilder::ResponseBuilder(const Request &request, const ConfigLocation &location)
	: req(request)
	, loc(location)
	, path("") {
}

ResponseBuilder::~ResponseBuilder() {
}
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

bool ResponseBuilder::shouldCloseConnection(int statusCode) {
	const std::string* conn = req.getHeader("connection");
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

HandlerResult ResponseBuilder::buildResponse( ) {
	HandlerResult result;
	const std::string& method = req.getMethod();

	bool methodAllowed = false;
	
	for (size_t i = 0; i < loc.allowedMethods.size(); ++i) {
		if (loc.allowedMethods[i] == method) {
			methodAllowed = true;
			break;
		}
	}
	
	if (!loc.allowedMethods.empty() && !methodAllowed) {
		result.staticResponse = handleError(405);
		return (result);
	}
	path = loc.locationRoot;

	if (path.empty())
		path = ".";

	if (path[path.size() - 1] == '/')
		path.erase(path.size() - 1);

	std::string uri = req.getUri();

	if (loc.path != "/" &&
		uri.compare(0, loc.path.length(), loc.path) == 0)
	{
		uri = uri.substr(loc.path.length());
	}

	if (!uri.empty() && uri[0] != '/')
		path += "/";

	path += uri;

	if (path.empty())
		path = ".";

	if (method == "GET")
		return (handleGet());
	if (method == "POST")
		return (handlePost());
	if (method == "DELETE")
		return (handleDelete());

	result.staticResponse = handleError(501);
	return (result);
}

bool ResponseBuilder::isCgiRequest() {
	std::string::size_type dotPos = path.rfind('.');
	if (dotPos == std::string::npos) {
		return false;
	}

	std::string ext = path.substr(dotPos);

	// 2. Cambiar cgiExtensions por cgiExtension (el nombre real en tu struct)
	for (size_t i = 0; i < loc.cgiExtension.size(); ++i) {
		if (loc.cgiExtension[i] == ext) {
			return true;
		}
	}

	return (false);
}

std::string ResponseBuilder::getCgiBinary() {
	std::string::size_type dotPos = path.rfind('.');
	if (dotPos == std::string::npos) {
		return ("");
	}

	std::string ext = path.substr(dotPos);

	// Los dos vectores deben tener el mismo tamaño
	size_t total = loc.cgiExtension.size();
	if (loc.cgiPath.size() < total) {
		total = loc.cgiPath.size();
	}

	for (size_t i = 0; i < total; ++i) {
		if (loc.cgiExtension[i] == ext) {
			return loc.cgiPath[i]; // Devuelve el binario configurado (ej: "/usr/bin/python3")
		}
	}

	return ("");
}