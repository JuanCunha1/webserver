#include "../../../include/protocol/ResponseBuilder.hpp"
#include "../../../include/protocol/MimeTypes.hpp"
#include "../../../include/protocol/CgiHandler.hpp"
#include "../../../include/Utils.hpp"

#include <sstream>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sys/stat.h>
#include <unistd.h>
#include <cstdlib>

//* Descodifica caracteres especiales (%XX) y espacios (+) de una cadena
static std::string urlDecode(const std::string &str) {
	std::string result;
	result.reserve(str.length());
	
	size_t i = 0;
	while (i < str.length()) {
		if (str[i] == '+') {
			result += ' ';
		} else if (str[i] == '%' && i + 2 < str.length()) {
			std::string hexStr = str.substr(i + 1, 2);
			char *endPtr;
			long decodedChar = std::strtol(hexStr.c_str(), &endPtr, 16);
			
			// Si la conversión fue exitosa
			if (*endPtr == '\0') {
				result += static_cast<char>(decodedChar);
				i += 2; // Saltamos los dos caracteres hexadecimales
			} else {
				result += str[i];
			}
		} else {
			result += str[i];
		}
		++i;
	}
	return (result);
}

//* Separa el body por '&' y '=', guardando en un mapa
static void parseUrlEncoded(const std::string &body, std::map<std::string, std::string> &formFields) {
	std::string::size_type start = 0;
	std::string::size_type end = body.find('&');

	while (start < body.length()) {
		std::string pair = body.substr(start, end - start);
		std::string::size_type eqPos = pair.find('=');

		if (eqPos != std::string::npos) {
			std::string key = urlDecode(pair.substr(0, eqPos));
			std::string value = urlDecode(pair.substr(eqPos + 1));
			formFields[key] = value;
		} else {
			// Caso donde hay clave pero no hay valor (ej: "clave&otra=2")
			std::string key = urlDecode(pair);
			formFields[key] = "";
		}

		if (end == std::string::npos) {
			break;
		}
		start = end + 1;
		end = body.find('&', start);
	}
}

//! Subida formulario, varios archivos, o imagen con varios parametros
//* El boundary es una cadena de texto que el navegador/cliente inventa
//* para saber dónde termina una parte y empieza la siguiente.
static std::string extractBoundary(const std::string &contentType) {
	std::string needle = "boundary=";
	size_t pos = contentType.find(needle);
	if (pos == std::string::npos) {
		return ("");
	}
	std::string boundary = contentType.substr(pos + needle.length());
	if (!boundary.empty() && boundary[0] == '"') {
		boundary = boundary.substr(1, boundary.find_last_of('"') - 1);
	}
	return ("--" + boundary);
}

//* Extrae el nombre de fichero(si lo hay) de cada parte del body
static std::string extractSanitizedFilename(const std::string &partHeaders) {
	std::string key = "filename=\"";
	size_t pos = partHeaders.find(key);
	if (pos == std::string::npos) {
		return ("");
	}
	pos += key.length();
	size_t endPos = partHeaders.find("\"", pos);
	if (endPos == std::string::npos) {
		return ("");
	}
	std::string filename = partHeaders.substr(pos, endPos - pos);
	size_t lastSlash = filename.find_last_of("/\\");
	if (lastSlash != std::string::npos) {
		filename = filename.substr(lastSlash + 1);
	}
	if (filename.empty() || filename == "." || filename == "..") {
		return ("");
	}
	return (filename);
}

//* Extrae el nombre del campo (name) de cada parte del body
//* Ignora las coincidencias con 'filename="'
static std::string extractFieldName(const std::string &partHeaders) {
	std::string key = "name=\"";
	size_t pos = 0;
	
	while ((pos = partHeaders.find(key, pos)) != std::string::npos) {
		// Comprobamos que no estemos haciendo match dentro de "filename="
		if (pos >= 4 && partHeaders.compare(pos - 4, 4, "file") == 0) {
			pos += key.length();
			continue; // Es un filename, seguimos buscando
		}
		
		pos += key.length();
		size_t endPos = partHeaders.find("\"", pos);
		if (endPos != std::string::npos) {
			return (partHeaders.substr(pos, endPos - pos));
		}
		break;
	}
	return ("");
}

//* Comprueba si una ruta de archivo ya existe en disco
static bool fileExists(const std::string &path) {
	struct stat buffer;
	return (stat(path.c_str(), &buffer) == 0);
}

//* Separa el nombre y la extensión, y busca un nombre único si ya existe
static std::string resolveUniqueFilename(const std::string &dir, const std::string &originalFilename) {
	std::string baseName = originalFilename;
	std::string extension = "";

	//* Comprobar punto no sea el primer caracter como .ignore
	size_t dotPos = originalFilename.find_last_of('.');
	if (dotPos != std::string::npos && dotPos != 0) {
		baseName = originalFilename.substr(0, dotPos);
		extension = originalFilename.substr(dotPos);
	}

	std::string candidateName = originalFilename;
	std::string fullPath = dir + candidateName;
	int counter = 1;

	//* Si ya existe por ejemplo _1
	while (fileExists(fullPath)) {
		std::ostringstream ss;
		ss << baseName << "_" << counter << extension;
		candidateName = ss.str();
		fullPath = dir + candidateName;
		counter++;
	}

	return (candidateName);
}

//* Abre un archivo con nombre único y escribe los datos
static bool saveFileToDisk(const std::string &dir, std::string &filename, const std::string &data) {
	std::string cleanDir = dir;
	if (cleanDir.empty() || cleanDir[cleanDir.size() - 1] != '/') {
		cleanDir += "/";
	}

	filename = resolveUniqueFilename(cleanDir, filename);
	std::string fullPath = cleanDir + filename;

	std::ofstream outFile(fullPath.c_str(), std::ios::out | std::ios::binary | std::ios::trunc);
	if (!outFile.is_open()) {
		return (false);
	}

	if (!data.empty()) {
		outFile.write(data.data(), data.size());
	}
	outFile.close();
	return (true);
}

//* Recorre el body HTTP buscando los boundary(cada bloque)
//* Extrae las cabeceras y datos, y los guarda en el disco los archivos detectados
//* Guardos los nombres de los archivos en un vector
static bool parseAndSaveMultipart(const std::string &body, const std::string &boundary,
									const std::string &uploadDir,
									std::vector<std::string> &savedFilenames,
									std::map<std::string, std::string> &formFields) {
	if (boundary.empty() || body.empty()) {
		return (false);
	}

	size_t currentPos = body.find(boundary);
	if (currentPos == std::string::npos) {
		return (false);
	}

	while (currentPos != std::string::npos) {
		currentPos += boundary.length();
		//* Si encuentra "--" indica fin del body
		if (currentPos + 2 <= body.size() && body.substr(currentPos, 2) == "--") {
			break;
		}
		//* Saltar el CRLF inicial
		if (currentPos + 2 <= body.size() && body.substr(currentPos, 2) == "\r\n") {
			currentPos += 2;
		}

		size_t headerEnd = body.find("\r\n\r\n", currentPos);
		if (headerEnd == std::string::npos) {
			break;
		}

		std::string partHeaders = body.substr(currentPos, headerEnd - currentPos);

		size_t dataStart = headerEnd + 4; // Saltar "\r\n\r\n"
		size_t nextBoundary = body.find(boundary, dataStart);
		if (nextBoundary == std::string::npos) {
			break;
		}

		size_t dataEnd = nextBoundary;
		if (dataEnd >= dataStart + 2 && body.substr(dataEnd - 2, 2) == "\r\n") {
			dataEnd -= 2;
		}

		std::string content = body.substr(dataStart, dataEnd - dataStart);
		std::string filename = extractSanitizedFilename(partHeaders);
		std::string fieldName = extractFieldName(partHeaders);
		if (!filename.empty()) {
			if (!saveFileToDisk(uploadDir, filename, content)) {
				return (false);
			}
			savedFilenames.push_back(filename);
		} else if (!fieldName.empty()) {
			formFields[fieldName] = content;
		}
		currentPos = nextBoundary;
	}
	return (!savedFilenames.empty() || !formFields.empty());
}

//* (cuando es text/plain)Guarda el body entero en el fichero indicado por la URL
Response ResponseBuilder::handlePostDirect() {
	struct stat statbuf;
	bool fileExisted = (stat(path.c_str(), &statbuf) == 0);

	if (fileExisted && S_ISDIR(statbuf.st_mode)) {
		return (buildErrorResponse(403, "Forbidden"));
	}
	if (fileExisted && access(path.c_str(), W_OK) == -1) {
		return (buildErrorResponse(403, "Forbidden"));
	}
	std::ofstream outFile(path.c_str(), std::ios::out | std::ios::binary | std::ios::trunc);
	if (!outFile.is_open()) {
		return (buildErrorResponse(500, "Internal Server Error"));
	}
	const std::string &body = req.getBody();
	if (!body.empty()) {
		outFile.write(body.data(), body.size());
	}
	outFile.close();

	Response res;
	if (!fileExisted) {
		res.setStatusCode(201);
		res.setStatusMessage("Created");
		res.setHeader("Location", req.getUri());

		std::string responseBody = "<html><body><h1>201 Created</h1><p>Resource created successfully.</p></body></html>";
		res.setHeader("Content-Type", "text/html");
		res.setHeader("Content-Length", Utils::toString(responseBody.size()));
		if (shouldCloseConnection(200)) {
			res.setHeader("Connection", "close");
		} else {
			res.setHeader("Connection", "keep-alive");
		}
		res.setBody(responseBody);
	} else {
		res.setStatusCode(200);
		res.setStatusMessage("OK");

		std::string responseBody = "<html><body><h1>200 OK</h1><p>Resource updated successfully.</p></body></html>";
		res.setHeader("Content-Type", "text/html");
		res.setHeader("Content-Length", Utils::toString(responseBody.size()));
		if (shouldCloseConnection(200)) {
			res.setHeader("Connection", "close");
		} else {
			res.setHeader("Connection", "keep-alive");
		}
		res.setBody(responseBody);
	}

	return (res);
}

HandlerResult ResponseBuilder::handlePost() {
    HandlerResult result;

    if (isCgiRequest()) {
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
            return (result); // Retorno asíncrono, sin while[cite: 1]
        }
    }

    const std::string *contentType = req.getHeader("content-type");
    if (contentType != NULL && contentType->find("multipart/form-data") != std::string::npos) {
        std::string boundary = extractBoundary(*contentType);
        std::vector<std::string> savedFilenames;
        std::map<std::string, std::string> formFields;

        if (!parseAndSaveMultipart(req.getBody(), boundary, path, savedFilenames, formFields)) {
            result.staticResponse = buildErrorResponse(400, "Bad Request");
            return (result);
        }

        Response res;
        if (!savedFilenames.empty()) {
            res.setStatusCode(201);
            res.setStatusMessage("Created");
            if (savedFilenames.size() == 1) {
                std::string uri = req.getUri();
                if (uri.empty() || uri[uri.size() - 1] != '/') uri += "/";
                res.setHeader("Location", uri + savedFilenames[0]);
            }
        } else {
            res.setStatusCode(200);
            res.setStatusMessage("OK");
        }

        std::string msg = "<html><body><h1>Procesado correctamente</h1></body></html>";
        res.setHeader("Content-Type", "text/html");
        res.setHeader("Content-Length", Utils::toString(msg.size()));
        if (shouldCloseConnection(200)) {
			res.setHeader("Connection", "close");
		} else {
			res.setHeader("Connection", "keep-alive");
		}
        res.setBody(msg);
        
        result.staticResponse = res;
        return (result);
    }
    else if (contentType != NULL && contentType->find("application/x-www-form-urlencoded") != std::string::npos) {
        std::map<std::string, std::string> formFields;
        parseUrlEncoded(req.getBody(), formFields);

        Response res;
        res.setStatusCode(200);
        res.setStatusMessage("OK");

        std::string msg = "<html><body><h1>Formulario procesado correctamente</h1></body></html>";
        res.setHeader("Content-Type", "text/html");
        res.setHeader("Content-Length", Utils::toString(msg.size()));
        if (shouldCloseConnection(200)) {
			res.setHeader("Connection", "close");
		} else {
			res.setHeader("Connection", "keep-alive");
		}
        res.setBody(msg);
        
        result.staticResponse = res;
        return (result);
    }

    result.staticResponse = handlePostDirect();
    return (result);
}