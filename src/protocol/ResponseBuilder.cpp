#include "../../include/protocol/ResponseBuilder.hpp"
#include "../../include/protocol/MimeTypes.hpp"
#include "./Utils.hpp"

#include <fstream>
#include <sstream>
#include <string>

//! Funciones que debere hacer
//isCgiRequest();
//handleCgi();

std::string readFile(const std::string& path) {
	std::ifstream file(path.c_str(), std::ios::in | std::ios::binary);
	if (!file.is_open())
		return "";
	
	std::ostringstream ss;
	ss << file.rdbuf();
	return (ss.str());
}

Response ResponseBuilder::handleGet(const Request &req, const std::string &path) {
	(void)req;
	Response res;
	
	if (isCgiRequest(path)) {
		return (handleCgi(path));
	}

	struct stat statbuf;
	if (stat(path.c_str(), &statbuf) == -1) {
		if (errno == ENOENT) {
			//* o hacer un buildErrorResponse(404, "Not Found");
			//* o mirar tema excepciones
			res.setStatusCode(404);
			res.setStatusMessage("Not found");
		}
		if (errno == EACCES) {
			res.setStatusCode(403);
			res.setStatusMessage("Forbidden");
		}
		res.setStatusCode(500);
		res.setStatusMessage("Internal server error");
	}

	if (S_ISDIR(statbuf.st_mode)) {
		//! Hacer comprobación de si existe index.html o si tengo activo el autoindex
		std::string indexPath = path + "/index.html"; // Asegúrate de formatear bien las barras
		struct stat indexStat;
		if (stat(indexPath.c_str(), &indexStat) == 0 && S_ISREG(indexStat.st_mode)) {
			return serveStaticFile(indexPath);
		}
		
		// Si autoindex está apagado o no hay index
		return buildErrorResponse(403, "Forbidden");
	}

	if (S_ISREG(statbuf.st_mode)) {
		if (access(path.c_str(), R_OK) == -1) {
			res.setStatusCode(403);
			res.setStatusMessage("Forbidden");
		}
		res.setStatusCode(200);
		res.setStatusMessage("OK");
		res.setHeader("Content-Type", MimeTypes::getType(filePath));
		res.setHeader("Content-Length", Utils::toString(readFile(path).size()));
		res.setBody(readFile(path));
		//Tambien me faltaria añadir los headers pero no se cuales poner
	}
	//! Otros casos no soportados
	res.setStatusCode(403);
	res.setStatusMessage("Forbidden");
}

/*
Response ResponseBuilder::handlePost(const Request &req, const std::string &path) {
	//* Comprobaciones
	if (isCgiRequest(path) = true) {
		handleCgi(path);
	}


	//* Ejecución

}

Response ResponseBuilder::handleDelete(const Request &req, const std::string &path) {
	//* Comprobaciones
	if (isCgiRequest(path) = true) {
		handleCgi(path);
	}


	//* Ejecución

}

Response ResponseBuilder::handleError(const Request &req, int errorCode) {

}

*/

Response ResponseBuilder::buildResponse(const Request &req, const std::string &path) {
	//* Comprobamos que metodo es, si no es ninguno codigo de error...
	const std::string& method = req.getMethod();

	if (method == "GET") {
		return (handleGet(req, path));
	} /*else if (method == "POST") {
		return (handlePost(req, path));
	} else if (method == "DELETE") {
		return (handleDelete(req, path));
	} else {
		return (handleError(req, 501)); // 501 Not Implemented
	}
		*/
}