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
#include <cerrno>
#include <cstring> // For strerror

std::string readFile(const std::string &path) {
	std::ifstream file(path.c_str(), std::ios::in | std::ios::binary);
	if (!file.is_open())
		return ("");
	/* // A: We comment for managing binary files (like images) correctly
	std::ostringstream ss;
	ss << file.rdbuf();
	return (ss.str());
    */
    // Calculate the total size of the file
    file.seekg(0, std::ios::end);
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    if (size <= 0)
    {
        return (""); // Empty file
    }
    // Initialize the buffer (std::string) with the exact size
    std::string buffer(size, '\0');
    // Read all binary bytes directly into the buffer
    if (file.read(&buffer[0], size)) {
        return (buffer);
    }
    return ("");
}


//! Both functions are to reduce lines in the handlers
//! Implement in the rest of the code
Response ResponseBuilder::serveStaticFile(const std::string &filePath) {
	Response res;
	std::string fileContent = readFile(filePath);

	res.setStatusCode(200);
	res.setStatusMessage("OK");
	res.setHeader("Content-Type", MimeTypes::getType(filePath));
	res.setHeader("Content-Length", Utils::toString(fileContent.size()));
	//! look at how to do it depending on whether the client stays (keep-alive) or closes
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

    //* Iterate over all the elements of the folder
    while ((ent = readdir(dir)) != NULL) {
        std::string filename = ent->d_name;

        // Skip the current directory "." to keep it cleaner, 
        if (filename == ".") {
            continue;
        }

        // Ensure the base URI ends in '/' to concatenate the link properly
        std::string href = requestUri;
        if (!href.empty() && href[href.length() - 1] != '/') {
            href += "/";
        }
        href += filename;

        // Add a visual '/' if the element is a directory
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
		//! Look into what this is
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
        // Extract the file extension
        std::string ext = "";
        std::string::size_type dotPos = path.rfind('.');
        if (dotPos != std::string::npos) {
            ext = path.substr(dotPos);
        }

        std::string cgiBinary;
        
        // Check whether the extension belongs to a CGI according to the .conf
        if (isCgiExtension(ext, cgiBinary)) {
            if (access(path.c_str(), F_OK) == -1) {
                result.staticResponse = handleError(404);
                return (result);
            }

            if (cgiBinary.empty() || cgiBinary == "\"\"" || cgiBinary == "''") {
                // It is a COMPILED CGI (e.g. ubuntu_cgi_tester)
                // To run it directly, it NEEDS execute permission (X_OK)
                if (access(path.c_str(), X_OK) == -1) {
                    std::cerr << "Fallo access(X_OK) compilado. Error: " << strerror(errno) << std::endl;
                    result.staticResponse = handleError(403);
                    return (result);
                }
            } else {
                std::cout << "-> Evaluando como SCRIPT INTERPRETADO (Python/PHP)" << std::endl;
                // It is an INTERPRETED SCRIPT (e.g. Python)
                // The script needs read permission (R_OK) for the interpreter to read it
                if (access(path.c_str(), R_OK) == -1) {
                    std::cerr << "Fallo access(R_OK) al script. Error: " << strerror(errno) << std::endl;
                    result.staticResponse = handleError(403);
                    return (result);
                }
                // And the interpreter (Python) needs execute permission (X_OK)
                if (access(cgiBinary.c_str(), X_OK) == -1) {
                    std::cerr << "Fallo access(X_OK) al interprete [" << cgiBinary << "]. Error: " << strerror(errno) << std::endl;
                    result.staticResponse = handleError(403);
                    return (result);
                }
            }

            // --- CGI EXECUTION ---
            result.isCgi = true;
            result.cgiHandler = new CgiHandler();
            
            if (!result.cgiHandler->initCgi(req, path, cgiBinary)) {
                delete result.cgiHandler;
                result.isCgi = false;
                result.staticResponse = handleError(500);
            }
            return (result); 
        }

        // --- STATIC FILE HANDLING ---
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
			// maybe look into exceptions
			return (buildErrorResponse(404, "Not Found"));
		}
		if (errno == EACCES) {
			return (buildErrorResponse(403, "Forbidden"));
		}
		return (buildErrorResponse(500, "Internal server error"));
	}

	if (S_ISDIR(statbuf.st_mode)) {
		//! Check whether index.html exists or whether autoindex is enabled
		std::string indexPath = path + "/index.html"; // Make sure to format the slashes properly
		struct stat indexStat;
		if (stat(indexPath.c_str(), &indexStat) == 0 && S_ISREG(indexStat.st_mode)) {
			return (serveStaticFile(req, indexPath));
		}
		// If autoindex is off or there is no index
		return (buildErrorResponse(403, "Forbidden"));
	}

	if (S_ISREG(statbuf.st_mode)) {
		// CGI part (waiting for Ainhoa)
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
	//! Other unsupported cases
	return (buildErrorResponse(403, "Forbidden"));
}
	*/