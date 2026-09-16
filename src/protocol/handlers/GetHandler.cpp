#include "protocol/ResponseBuilder.hpp"
#include "protocol/MimeTypes.hpp"
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

Response ResponseBuilder::handleGet(const Request &req, const std::string &path) {	
	//! He visto que la comprobación de si es CGI la deberia hacer desde el .conf
	/*
	if (isCgiRequest(path)) {
		return (CgiHandler::initCgi(req, path)); 
	}*/

	struct stat statbuf;
	if (stat(path.c_str(), &statbuf) == -1) {
		if (errno == ENOENT) {
			//* igual mirar tema excepciones
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
		if (access(path.c_str(), R_OK) == -1) {
			return (buildErrorResponse(403, "Forbidden"));
		}
		return (serveStaticFile(req, path));
	}
	//! Otros casos no soportados
	return (buildErrorResponse(403, "Forbidden"));
}