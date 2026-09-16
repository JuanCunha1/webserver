#include "../../../include/protocol/ResponseBuilder.hpp"

#include <sys/stat.h>
#include <unistd.h>
#include <cerrno>
#include <string>

//* Función auxiliar para obtener la ruta del directorio que contiene al archivo
static std::string getParentDirectory(const std::string &path) {
	size_t lastSlash = path.find_last_of('/');
	if (lastSlash == std::string::npos) {
		return (".");
	}
	if (lastSlash == 0) {
		return ("/");
	}
	return (path.substr(0, lastSlash));
}

Response ResponseBuilder::handleDelete(const Request &req, const std::string &path) {
	(void)req;
	struct stat statbuf;

	if (stat(path.c_str(), &statbuf) == -1) {
		if (errno == ENOENT) {
			return (buildErrorResponse(404, "Not Found"));
		}
		if (errno == EACCES) {
			return (buildErrorResponse(403, "Forbidden"));
		}
		return (buildErrorResponse(500, "Internal Server Error"));
	}
	if (S_ISDIR(statbuf.st_mode)) {
		return (buildErrorResponse(403, "Forbidden"));
	}
	std::string parentDir = getParentDirectory(path);
	if (access(parentDir.c_str(), W_OK) == -1) {
		return (buildErrorResponse(403, "Forbidden"));
	}
	if (unlink(path.c_str()) == -1) {
		if (errno == EACCES) {
			return (buildErrorResponse(403, "Forbidden"));
		}
		// Si hay otro tipo de error al borrar, devolvemos 500
		return (buildErrorResponse(500, "Internal Server Error"));
	}
	Response res;
	res.setStatusCode(204);
	res.setStatusMessage("No Content");
	res.setHeader("Connection", "keep-alive");
	res.setHeader("Content-Length", "0");
	res.setBody("");

	return (res);
}