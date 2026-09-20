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
Response ResponseBuilder::serveStaticFile(const Request &req, const std::string &filePath) {
	Response res;
	std::string fileContent = readFile(filePath);

	res.setStatusCode(200);
	res.setStatusMessage("OK");
	res.setHeader("Content-Type", MimeTypes::getType(filePath));
	res.setHeader("Content-Length", Utils::toString(fileContent.size()));
	//! mira como hacer dependiendo si cliente sigue(keep-alive) o hace un close
	if (shouldCloseConnection(req, 200)) {
		res.setHeader("Connection", "close");
	} else {
		res.setHeader("Connection", "keep-alive");
	}
	res.setBody(fileContent);

	return (res);
}

Response ResponseBuilder::handleGet(const Request &req, const std::string &path, const ConfigLocation &loc) {
    struct stat statbuf;

    // 1. Comprobar existencia del recurso
    if (stat(path.c_str(), &statbuf) == -1) {
        if (errno == ENOENT) {
            return (buildErrorResponse(404, "Not Found"));
        }
        if (errno == EACCES) {
            return (buildErrorResponse(403, "Forbidden"));
        }
        return (buildErrorResponse(500, "Internal Server Error"));
    }

    // 2. Si es Directorio
    if (S_ISDIR(statbuf.st_mode)) {
        std::string indexPath = path;
        if (!indexPath.empty() && indexPath[indexPath.size() - 1] != '/') {
            indexPath += "/";
        }
        indexPath += loc.indexFile.empty() ? "index.html" : loc.indexFile;

        struct stat indexStat;
        if (stat(indexPath.c_str(), &indexStat) == 0 && S_ISREG(indexStat.st_mode)) {
            return (serveStaticFile(req, indexPath));
        }

        if (loc.autoindex) {
            // return (generateAutoindex(req, path));
        }
        return (buildErrorResponse(403, "Forbidden"));
    }

    // 3. Si es Archivo Regular
    if (S_ISREG(statbuf.st_mode)) {
        std::string cgiBinary = getCgiBinary(path, loc);

        // Si cgiBinary no está vacío, es una petición CGI configurada
        if (!cgiBinary.empty()) {
            // El script debe tener permisos de lectura
            if (access(path.c_str(), R_OK) == -1) {
                return (buildErrorResponse(403, "Forbidden"));
            }
            // El binario ejecutable (ej: /usr/bin/python3) debe existir y poder ejecutarse
            if (access(cgiBinary.c_str(), X_OK) == -1) {
                return (buildErrorResponse(500, "Internal Server Error"));
            }
            
            // Le pasas la petición, la ruta del script y la ruta del binario
            return (CgiHandler::initCgi(req, path, cgiBinary));
        }

        // Si no es CGI, se sirve como estático
        if (access(path.c_str(), R_OK) == -1) {
            return (buildErrorResponse(403, "Forbidden"));
        }
        return (serveStaticFile(req, path));
    }

    return (buildErrorResponse(403, "Forbidden"));
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