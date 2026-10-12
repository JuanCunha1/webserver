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

HandlerResult ResponseBuilder::buildResponse( ) {
	HandlerResult result;

	//* Redirection handling
	if (!loc.returnRedirections.empty()) {
		Response res;
		int redirectCode = loc.returnRedirections[0].returnCode;
				
		// If the conf has "return /new-route;" without a code, the default is 302
		if (redirectCode == 0) {
			redirectCode = 302;
		}

		res.setStatusCode(redirectCode);
		// Assuming getDefaultStatusMessage has been moved so it is accessible
		res.setStatusMessage(getDefaultStatusMessage(redirectCode)); 
		
		// The key header so the browser knows where to go
		res.setHeader("Location", loc.returnRedirections[0].returnUrl);
		res.setHeader("Content-Length", "0"); 
		res.setHeader("Connection", "keep-alive");

		result.staticResponse = res;
		return (result);
	}

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

	/* // A: change of order to solve the problem of path traversal, first normalize the URI, then strip the location prefix
	if (loc.path != "/" &&
		uri.compare(0, loc.path.length(), loc.path) == 0)
	{
		uri = uri.substr(loc.path.length());
	}
	*/

	//* Path Traversal prevention (normalizeUri - the one that verifies)
	std::string cleanUri = req.getUri();

	cleanUri = Utils::urlDecode(cleanUri);
	//std::cout << "Decoded: " << cleanUri << std::endl;
	cleanUri = normalizeUri(cleanUri);
	//std::cout << "Normalized: " << cleanUri << std::endl;

	// 2. If normalization returns empty but the original URI was not, it's a traversal attempt
	if (cleanUri.empty() && !req.getUri().empty()) {
		result.staticResponse = handleError(403);
		return (result);
	}
	//uri = cleanUri;
	//* up to here
	// NOW, on the already clean URI, strip the location prefix
	std::string uri = cleanUri;
	if (loc.path != "/" && uri.compare(0, loc.path.length(), loc.path) == 0) {
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