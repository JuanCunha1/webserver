#include "protocol/ResponseBuilder.hpp"
#include "protocol/MimeTypes.hpp"
#include "Utils.hpp"

#include <fstream>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

ResponseBuilder::ResponseBuilder(const Request &request, const ConfigLocation &location, const ConfigServer &server)
	: req(request)
	, loc(location)
	, server(server)
	, path("") {
}

ResponseBuilder::~ResponseBuilder() {
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

//! Orchestrator: redirection -> allowed method -> resolve path -> dispatch
HandlerResult ResponseBuilder::buildResponse( ) {
	HandlerResult result;

	//* Redirection handling
	if (!loc.returnRedirections.empty()) {
		result.staticResponse = handleRedirection();
		return (result);
	}

	const std::string& method = req.getMethod();

	if (!isMethodAllowed(method)) {
		result.staticResponse = handleError(405);
		return (result);
	}

	//* Path Traversal prevention: builds 'path' from root + clean URI
	if (!resolvePath()) {
		result.staticResponse = handleError(403);
		return (result);
	}

	return (dispatchMethod(method));
}

Response ResponseBuilder::handleRedirection() {
	Response res;
	int redirectCode = loc.returnRedirections[0].returnCode;

	if (redirectCode == 0) {
		redirectCode = 302;
	}

	res.setStatusCode(redirectCode);
	res.setStatusMessage(getDefaultStatusMessage(redirectCode));

	res.setHeader("Location", loc.returnRedirections[0].returnUrl);
	res.setHeader("Content-Length", "0");
	res.setHeader("Connection", "keep-alive");

	return (res);
}

// If the location has no allowed methods configured, everything is allowed
bool ResponseBuilder::isMethodAllowed(const std::string &method) const {
	if (loc.allowedMethods.empty()) {
		return (true);
	}
	for (size_t i = 0; i < loc.allowedMethods.size(); ++i) {
		if (loc.allowedMethods[i] == method) {
			return (true);
		}
	}
	return (false);
}

// Builds the 'path' member: locationRoot + normalized URI (without the location prefix)
// Returns false if the URI tries to escape the root (path traversal)
bool ResponseBuilder::resolvePath() {
	path = loc.locationRoot;

	if (path.empty())
		path = ".";

	if (path[path.size() - 1] == '/')
		path.erase(path.size() - 1);

	//* Path Traversal prevention (normalizeUri - the one that verifies)
	//* First normalize the URI, then strip the location prefix
	std::string cleanUri = req.getUri();

	cleanUri = Utils::urlDecode(cleanUri);
	cleanUri = normalizeUri(cleanUri);

	if (cleanUri.empty() && !req.getUri().empty()) {
		return (false);
	}

	std::string uri = stripLocationPrefix(cleanUri);

	if (!uri.empty() && uri[0] != '/')
		path += "/";

	path += uri;

	if (path.empty())
		path = ".";

	return (true);
}

// "/uploads/photo.png" with location "/uploads" -> "/photo.png"
std::string ResponseBuilder::stripLocationPrefix(const std::string &uri) const {
	if (loc.path != "/" && uri.compare(0, loc.path.length(), loc.path) == 0) {
		return (uri.substr(loc.path.length()));
	}
	return (uri);
}

HandlerResult ResponseBuilder::dispatchMethod(const std::string &method) {
	HandlerResult result;

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

	for (size_t i = 0; i < loc.cgiExtension.size(); ++i) {
		if (loc.cgiExtension[i] == ext) {
			return true;
		}
	}

	return (false);
}

bool ResponseBuilder::isCgiExtension(const std::string& ext, std::string& outCgiBinary) {
	size_t total = loc.cgiExtension.size();
	if (loc.cgiPath.size() < total) {
		total = loc.cgiPath.size();
	}

	for (size_t i = 0; i < total; ++i) {
		if (loc.cgiExtension[i] == ext) {
			outCgiBinary = loc.cgiPath[i];
			return true;
		}
	}

	return false;
}

//* Cleans the ./ and resolves the ../
std::string ResponseBuilder::normalizeUri(const std::string& uri) {
	std::vector<std::string> parts;
	size_t start = 0;

	while (start < uri.length()) {
		size_t end = uri.find('/', start);
		std::string part = (end == std::string::npos) ? uri.substr(start) : uri.substr(start, end - start);

		if (part == "..") {
			if (!parts.empty()) {
				parts.pop_back(); // Remove the previous valid folder
			} else {
				return ""; // Path Traversal attempt blocked
			}
		} else if (!part.empty() && part != ".") {
			parts.push_back(part); // Store the valid directory
		}

		if (end == std::string::npos) break;
		start = end + 1;
	}

	std::string normalized = "";
	for (size_t i = 0; i < parts.size(); ++i) {
		normalized += "/" + parts[i];
	}
	
	return normalized.empty() ? "/" : normalized;
}