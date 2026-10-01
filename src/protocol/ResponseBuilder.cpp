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

	//* Gestión redirecciones
	if (!loc.returnRedirections.empty()) {
		Response res;
		int redirectCode = loc.returnRedirections[0].returnCode;
				
		// Si en el conf pusieron "return /nueva-ruta;" sin código, por defecto es 302
		if (redirectCode == 0) {
			redirectCode = 302;
		}

		res.setStatusCode(redirectCode);
		// Asumiendo que has movido getDefaultStatusMessage para que sea accesible
		res.setStatusMessage(getDefaultStatusMessage(redirectCode)); 
		
		// El header clave para que el navegador sepa a dónde ir
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

	std::string uri = req.getUri();

	if (loc.path != "/" &&
		uri.compare(0, loc.path.length(), loc.path) == 0)
	{
		uri = uri.substr(loc.path.length());
	}

	//* Prevención Path Traversal (normalizeUri - el que verifica)
	std::string cleanUri = req.getUri();

	cleanUri = Utils::urlDecode(cleanUri);
	//std::cout << "Decoded: " << cleanUri << std::endl;
	cleanUri = normalizeUri(cleanUri);
	//std::cout << "Normalized: " << cleanUri << std::endl;

	//* Si normalizeUri devuelve "" pero la uri original no lo era, es un intento de escape (malicoso)
	if (cleanUri.empty() && !uri.empty()) {
		result.staticResponse = handleError(403);
		return (result);
	}
	uri = cleanUri;
	//* hasta aquí

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

//* Limpia los ./ y resuelve los ../
std::string ResponseBuilder::normalizeUri(const std::string& uri) {
    std::vector<std::string> parts;
    size_t start = 0;

    while (start < uri.length()) {
        size_t end = uri.find('/', start);
        std::string part = (end == std::string::npos) ? uri.substr(start) : uri.substr(start, end - start);

        if (part == "..") {
            if (!parts.empty()) {
                parts.pop_back(); // Elimina la carpeta anterior válida
            } else {
                return ""; // Intento de Path Traversal bloqueado
            }
        } else if (!part.empty() && part != ".") {
            parts.push_back(part); // Guarda el directorio válido
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