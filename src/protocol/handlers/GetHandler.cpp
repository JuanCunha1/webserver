#include "protocol/ResponseBuilder.hpp"
#include "protocol/MimeTypes.hpp"
#include "protocol/CgiHandler.hpp"
#include "Utils.hpp"

#include <fstream>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

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

std::string generateAutoindex(const std::string& directoryPath, const std::string& requestUri) {
    DIR *dir;
    struct dirent *ent;
    std::ostringstream html;

    dir = opendir(directoryPath.c_str());
    if (dir == NULL) {
        return "";
    }

    html << "<html>\r\n<head><title>Index of " << requestUri << "</title></head>\r\n"
         << "<body style=\"font-family: monospace;\">\r\n"
         << "<h1>Index of " << requestUri << "</h1>\r\n<hr>\r\n<pre>\r\n";

    //* Iteramos sobre todos los elementos de la carpeta
    while ((ent = readdir(dir)) != NULL) {
        std::string filename = ent->d_name;

        // Omitir el directorio actual "." para que quede más limpio, 
        if (filename == ".") {
            continue;
        }

        // Asegurar que la URI base termina en '/' para concatenar bien el link
        std::string href = requestUri;
        if (!href.empty() && href[href.length() - 1] != '/') {
            href += "/";
        }
        href += filename;

        // Añadir un '/' visual si el elemento es un directorio
        std::string displayName = filename;
        if (ent->d_type == DT_DIR) {
            displayName += "/";
        }

        html << "<a href=\"" << href << "\">" << displayName << "</a>\n";
    }

    closedir(dir);
    html << "</pre>\r\n<hr>\r\n</body>\r\n</html>\r\n";

    return html.str();
}

HandlerResult ResponseBuilder::handleGet() {
    HandlerResult result;
    struct stat statbuf;
    if (stat(path.c_str(), &statbuf) == -1) {
        if (errno == ENOENT) {
            result.staticResponse = handleError(404);
			//std::cout << "AQUI" << std::endl;
        } else if (errno == EACCES) {
            result.staticResponse = handleError(403);
        } else {
            result.staticResponse = handleError(500);
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
            std::string autoindexHtml = generateAutoindex(path, req.getUri());
        
            if (!autoindexHtml.empty()) {
                Response res;
                res.setStatusCode(200);
                res.setStatusMessage("OK");
                res.setHeader("Content-Type", "text/html");
                res.setHeader("Content-Length", Utils::toString(autoindexHtml.size()));
                res.setBody(autoindexHtml);
                
                HandlerResult result;
                result.staticResponse = res;
                return result;
            } else {
                result.staticResponse = handleError(403);
                return (result); 
            }
        } else {
            result.staticResponse = handleError(403);
            return (result);
        }
        result.staticResponse = handleError(403);
        return (result);
    }

    if (S_ISREG(statbuf.st_mode)) {
        std::string cgiBinary = getCgiBinary();
		/*
		// --- INICIO DEBUG ---
        std::cout << "\n[DEBUG CGI] ------------------------" << std::endl;
        std::cout << "[DEBUG CGI] Archivo solicitado: " << path << std::endl;
        std::cout << "[DEBUG CGI] Binario CGI detectado: '" << cgiBinary << "'" << std::endl;
        std::cout << "[DEBUG CGI] ------------------------\n" << std::endl;
        // --- FIN DEBUG ---
		*/
        if (!cgiBinary.empty()) {
            if (access(path.c_str(), R_OK) == -1 || access(cgiBinary.c_str(), X_OK) == -1) {
                result.staticResponse = handleError(403);
                return (result);
            }
            
            result.isCgi = true;
            result.cgiHandler = new CgiHandler();
            
            if (!result.cgiHandler->initCgi(req, path, cgiBinary)) {
                delete result.cgiHandler;
                result.isCgi = false;
                result.staticResponse = handleError(500);
            }
            return (result); // Retorno asíncrono, sin while[cite: 2]
        }

        if (access(path.c_str(), R_OK) == -1) {
            result.staticResponse = handleError(403);
            return (result);
        }
        
        result.staticResponse = serveStaticFile(path);
        return (result);
    }

    result.staticResponse = handleError(403);
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