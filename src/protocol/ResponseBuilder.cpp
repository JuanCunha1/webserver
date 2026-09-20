#include "protocol/ResponseBuilder.hpp"
#include "protocol/MimeTypes.hpp"
#include "Utils.hpp"

#include <fstream>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

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

Response ResponseBuilder::buildResponse(const Request &req, const std::string &path, const ConfigLocation &loc) {
	//* Comprobamos que metodo es, si no es ninguno codigo de error...
	const std::string& method = req.getMethod();


	//! No se porque
	bool methodAllowed = false;
    for (size_t i = 0; i < loc.allowedMethods.size(); ++i) {
        if (loc.allowedMethods[i] == method) {
            methodAllowed = true;
            break;
        }
    }
    if (!loc.allowedMethods.empty() && !methodAllowed) {
        return (buildErrorResponse(405, "Method Not Allowed"));
    }

	if (method == "GET") {
		return (handleGet(req, path, loc));
	} else if (method == "POST") {
		return (handlePost(req, path, loc));
	} else if (method == "DELETE") {
		return (handleDelete(req, path));
	}  else {
		return (handleError(req, 501));
	}
}

bool ResponseBuilder::isCgiRequest(const std::string &path, const ConfigLocation &loc) {
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

    return false;
}

std::string ResponseBuilder::getCgiBinary(const std::string &path, const ConfigLocation &loc) {
    std::string::size_type dotPos = path.rfind('.');
    if (dotPos == std::string::npos) {
        return "";
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

    return "";
}