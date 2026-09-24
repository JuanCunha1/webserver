#include "protocol/ResponseBuilder.hpp"
#include "protocol/MimeTypes.hpp"
#include "protocol/CgiHandler.hpp"
#include "Utils.hpp"

#include <fstream>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

std::string readFile(const std::string &path) {
	std::ifstream file(path.c_str(), std::ios::in | std::ios::binary);
	if (!file.is_open())
		return ("");
	
	std::ostringstream ss;
	ss << file.rdbuf();
	return (ss.str());
}

//! Ambas funciones son para reducir lines en los handle
//! Implementar en el resto del codigo
Response ResponseBuilder::serveStaticFile(const std::string &filePath) {
	Response res;
	std::string fileContent = readFile(filePath);

	res.setStatusCode(200);
	res.setStatusMessage("OK");
	res.setHeader("Content-Type", MimeTypes::getType(filePath));
	res.setHeader("Content-Length", Utils::toString(fileContent.size()));
	//! mira como hacer dependiendo si cliente sigue(keep-alive) o hace un close
	if (shouldCloseConnection(200)) {
		res.setHeader("Connection", "close");
	} else {
		res.setHeader("Connection", "keep-alive");
	}
	res.setBody(fileContent);

	return (res);
}

HandlerResult ResponseBuilder::handleGet() {
    HandlerResult result;
    struct stat statbuf;
    if (stat(path.c_str(), &statbuf) == -1) {
        if (errno == ENOENT) {
            result.staticResponse = buildErrorResponse(404, "Not Found");
        } else if (errno == EACCES) {
            result.staticResponse = buildErrorResponse(403, "Forbidden");
        } else {
            result.staticResponse = buildErrorResponse(500, "Internal Server Error");
        }
        return (result);
    }

    if (S_ISDIR(statbuf.st_mode)) {
        std::string indexPath = path;
        if (!indexPath.empty() && indexPath[indexPath.size() - 1] != '/') {
            indexPath += "/";
        }
        indexPath += loc.indexFile.empty() ? "index.html" : loc.indexFile;

        struct stat indexStat;
        if (stat(indexPath.c_str(), &indexStat) == 0 && S_ISREG(indexStat.st_mode)) {
            result.staticResponse = serveStaticFile(indexPath);
            return (result);
        }
		//! Mirar que es esto
        if (loc.autoindex) {
            // result.staticResponse = generateAutoindex(req, path);
            // return (result);
        }
        
        result.staticResponse = buildErrorResponse(403, "Forbidden");
        return (result);
    }

    if (S_ISREG(statbuf.st_mode)) {
        std::string cgiBinary = getCgiBinary();

        if (!cgiBinary.empty()) {
            if (access(path.c_str(), R_OK) == -1 || access(cgiBinary.c_str(), X_OK) == -1) {
                result.staticResponse = buildErrorResponse(403, "Forbidden");
                return (result);
            }
            
            result.isCgi = true;
            result.cgiHandler = new CgiHandler();
            
            if (!result.cgiHandler->initCgi(req, path, cgiBinary)) {
                delete result.cgiHandler;
                result.isCgi = false;
                result.staticResponse = buildErrorResponse(500, "Internal Server Error");
            }
            return (result); // Retorno asíncrono, sin while[cite: 2]
        }

        if (access(path.c_str(), R_OK) == -1) {
            result.staticResponse = buildErrorResponse(403, "Forbidden");
            return (result);
        }
        
        result.staticResponse = serveStaticFile(path);
        return (result);
    }

    result.staticResponse = buildErrorResponse(403, "Forbidden");
    return (result);
}

/*
Response ResponseBuilder::handleGet(const Request &req, const std::string &path) {	

	struct stat statbuf;
	if (stat(path.c_str(), &statbuf) == -1) {
		if (errno == ENOENT) {
			// igual mirar tema excepciones
			return (buildErrorResponse(404, "Not Found"));
		}
		if (errno == EACCES) {
			return (buildErrorResponse(403, "Forbidden"));
		}
		return (buildErrorResponse(500, "Internal server error"));
	}

	if (S_ISDIR(statbuf.st_mode)) {
		//! Hacer comprobación de si existe index.html o si tengo activo el autoindex
		std::string indexPath = path + "/index.html"; // Asegúrate de formatear bien las barras
		struct stat indexStat;
		if (stat(indexPath.c_str(), &indexStat) == 0 && S_ISREG(indexStat.st_mode)) {
			return (serveStaticFile(req, indexPath));
		}
		// Si autoindex está apagado o no hay index
		return (buildErrorResponse(403, "Forbidden"));
	}

	if (S_ISREG(statbuf.st_mode)) {
		// Parte CGI (a la espera de Ainhoa)
		if (isCgiRequest(path)) {
            if (access(path.c_str(), R_OK) == -1) {
                return (buildErrorResponse(403, "Forbidden"));
            }
            return (CgiHandler::initCgi(req, path));
        }
		if (access(path.c_str(), R_OK) == -1) {
			return (buildErrorResponse(403, "Forbidden"));
		}
		return (serveStaticFile(req, path));
	}
	//! Otros casos no soportados
	return (buildErrorResponse(403, "Forbidden"));
}
	*/