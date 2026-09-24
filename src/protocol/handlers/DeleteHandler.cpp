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

HandlerResult ResponseBuilder::handleDelete(const Request &req, const std::string &path) {
    (void)req;
    HandlerResult result;
    struct stat statbuf;

    if (stat(path.c_str(), &statbuf) == -1) {
        if (errno == ENOENT) result.staticResponse = buildErrorResponse(404, "Not Found");
        else if (errno == EACCES) result.staticResponse = buildErrorResponse(403, "Forbidden");
        else result.staticResponse = buildErrorResponse(500, "Internal Server Error");
        return (result);
    }
    
    if (S_ISDIR(statbuf.st_mode)) {
        result.staticResponse = buildErrorResponse(403, "Forbidden");
        return (result);
    }
    
    std::string parentDir = getParentDirectory(path);
    if (access(parentDir.c_str(), W_OK) == -1) {
        result.staticResponse = buildErrorResponse(403, "Forbidden");
        return (result);
    }
    
    if (unlink(path.c_str()) == -1) {
        if (errno == EACCES) result.staticResponse = buildErrorResponse(403, "Forbidden");
        else result.staticResponse = buildErrorResponse(500, "Internal Server Error");
        return (result);
    }
    
    Response res;
    res.setStatusCode(204);
    res.setStatusMessage("No Content");
    res.setHeader("Connection", "keep-alive");
    //res.setHeader("Content-Length", "0"); para cumplir el RFC 7230
    res.setBody("");

    result.staticResponse = res;
    return (result);
}