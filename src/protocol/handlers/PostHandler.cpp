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

//* Decodes special characters (%XX) and spaces (+) of a string
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
			
			// If the conversion was successful
			if (*endPtr == '\0') {
				result += static_cast<char>(decodedChar);
				i += 2; // Skip the two hexadecimal characters
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

//* Splits the body by '&' and '=', storing in a map
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
			// Case where there is a key but no value (e.g. "key&other=2")
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

//! Form upload, several files, or image with several parameters
//* The boundary is a text string that the browser/client makes up
//* to know where one part ends and the next begins.
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

//* Extracts the file name (if any) of each part of the body
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

//* Extracts the field name (name) of each part of the body
//* Ignores matches with 'filename="'
static std::string extractFieldName(const std::string &partHeaders) {
	std::string key = "name=\"";
	size_t pos = 0;
	
	while ((pos = partHeaders.find(key, pos)) != std::string::npos) {
		// Check that we are not matching inside "filename="
		if (pos >= 4 && partHeaders.compare(pos - 4, 4, "file") == 0) {
			pos += key.length();
			continue; // It is a filename, keep searching
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

//* Checks whether a file path already exists on disk
static bool fileExists(const std::string &path) {
	struct stat buffer;
	return (stat(path.c_str(), &buffer) == 0);
}

//* Splits the name and the extension, and looks for a unique name if it already exists
static std::string resolveUniqueFilename(const std::string &dir, const std::string &originalFilename) {
	std::string baseName = originalFilename;
	std::string extension = "";

	//* Check the dot is not the first character, as in .ignore
	size_t dotPos = originalFilename.find_last_of('.');
	if (dotPos != std::string::npos && dotPos != 0) {
		baseName = originalFilename.substr(0, dotPos);
		extension = originalFilename.substr(dotPos);
	}

	std::string candidateName = originalFilename;
	std::string fullPath = dir + candidateName;
	int counter = 1;

	//* If it already exists, for example _1
	while (fileExists(fullPath)) {
		std::ostringstream ss;
		ss << baseName << "_" << counter << extension;
		candidateName = ss.str();
		fullPath = dir + candidateName;
		counter++;
	}

	return (candidateName);
}

//* Opens a file with a unique name and writes the data
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

//* Walks through the HTTP body looking for the boundaries (each block)
//* Extracts the headers and data, and saves the detected files to disk
//* Stores the file names in a vector
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
		//* If it finds "--" it indicates the end of the body
		if (currentPos + 2 <= body.size() && body.substr(currentPos, 2) == "--") {
			break;
		}
		//* Skip the initial CRLF
		if (currentPos + 2 <= body.size() && body.substr(currentPos, 2) == "\r\n") {
			currentPos += 2;
		}

		size_t headerEnd = body.find("\r\n\r\n", currentPos);
		if (headerEnd == std::string::npos) {
			break;
		}

		std::string partHeaders = body.substr(currentPos, headerEnd - currentPos);

		size_t dataStart = headerEnd + 4; // Skip "\r\n\r\n"
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

//! HANDLE POST
// Orchestrator: CGI -> multipart -> urlencoded -> direct (raw body)
HandlerResult ResponseBuilder::handlePost() {
	HandlerResult result;
 
	std::string ext = "";
	std::string::size_type dotPos = path.rfind('.');
	if (dotPos != std::string::npos) {
		ext = path.substr(dotPos);
	}
 
	std::string cgiBinary;
 
	// 1. Check whether it is a CGI extension (like .bla, .py, .php)
	if (isCgiExtension(ext, cgiBinary)) {
		return (launchCgi(cgiBinary)); // Same function used by GET
	}
 
	const std::string *contentType = req.getHeader("content-type");
	if (contentType != NULL && contentType->find("multipart/form-data") != std::string::npos) {
		return (handleMultipart(*contentType));
	}
	else if (contentType != NULL && contentType->find("application/x-www-form-urlencoded") != std::string::npos) {
		return (handleUrlEncoded());
	}
 
	result.staticResponse = handlePostDirect();
	return (result);
}
 
// Builds a text/html response with the Connection header
Response ResponseBuilder::buildPostResponse(int statusCode, const std::string &statusMsg,
											const std::string &html) {
	Response res;
	res.setStatusCode(statusCode);
	res.setStatusMessage(statusMsg);
	res.setHeader("Content-Type", "text/html");
	res.setHeader("Content-Length", Utils::toString(html.size()));
	if (shouldCloseConnection(200)) {
		res.setHeader("Connection", "close");
	} else {
		res.setHeader("Connection", "keep-alive");
	}
	res.setBody(html);
	return (res);
}
 
//! MULTIPART 
HandlerResult ResponseBuilder::handleMultipart(const std::string &contentType) {
	HandlerResult result;
	std::string boundary = extractBoundary(contentType);
	std::vector<std::string> savedFilenames;
	std::map<std::string, std::string> formFields;
 
	std::string targetDir = resolveUploadDir();
 
	if (!parseAndSaveMultipart(req.getBody(), boundary, targetDir, savedFilenames, formFields)) {
		result.staticResponse = handleError(400);
		return (result);
	}
 
	std::string msg = "<html><body><h1>Procesado correctamente</h1></body></html>\n";
 
	if (savedFilenames.empty()) {
		result.staticResponse = buildPostResponse(200, "OK", msg);
		return (result);
	}
 
	Response res = buildPostResponse(201, "Created", msg);
	if (savedFilenames.size() == 1) {
		std::string uri = req.getUri();
		if (uri.empty() || uri[uri.size() - 1] != '/') uri += "/";
		res.setHeader("Location", uri + savedFilenames[0]);
	}
	result.staticResponse = res;
	return (result);
}
 
// Destination directory: upload_store if configured, otherwise root + URI
std::string ResponseBuilder::resolveUploadDir() const {
	std::string targetDir = path; // Default: root + URI
 
	if (!loc.uploadStore.empty()) {
		targetDir = loc.uploadStore; // If there is an upload_store, use that one
 
		// Make sure it ends in '/' so concatenating the name does not fail
		if (targetDir[targetDir.size() - 1] != '/') {
			targetDir += "/";
		}
	}
	return (targetDir);
}
 
//! URL ENCODED 
HandlerResult ResponseBuilder::handleUrlEncoded() {
	HandlerResult result;
	std::map<std::string, std::string> formFields;
	parseUrlEncoded(req.getBody(), formFields);
 
	std::string msg = "<html><body><h1>Formulario procesado correctamente</h1></body></html>\n";
	result.staticResponse = buildPostResponse(200, "OK", msg);
	return (result);
}
 
//! DIRECT 
//* (when it is text/plain) Saves the whole body in the file indicated by the URL
Response ResponseBuilder::handlePostDirect() {
	std::string targetFilePath = resolveDirectTargetPath();
 
	struct stat statbuf;
	bool fileExisted = (stat(targetFilePath.c_str(), &statbuf) == 0);
 
	if (fileExisted && S_ISDIR(statbuf.st_mode)) {
		return (handleError(403));
	}
	if (fileExisted && access(targetFilePath.c_str(), W_OK) == -1) {
		return (handleError(403));
	}
 
	// Open the file at targetFilePath
	std::ofstream outFile(targetFilePath.c_str(), std::ios::out | std::ios::binary | std::ios::trunc);
	if (!outFile.is_open()) {
		return (handleError(500));
	}
	const std::string &body = req.getBody();
	if (!body.empty()) {
		outFile.write(body.data(), body.size());
	}
	outFile.close();
 
	if (!fileExisted) {
		std::string responseBody = "<html><body><h1>201 Created</h1><p>Resource created successfully.</p></body></html>\n";
		Response res = buildPostResponse(201, "Created", responseBody);
		// The 'Location' header still returns the original URI, which is correct from the client's point of view
		res.setHeader("Location", req.getUri());
		return (res);
	}
 
	std::string responseBody = "<html><body><h1>200 OK</h1><p>Resource updated successfully.</p></body></html>\n";
	return (buildPostResponse(200, "OK", responseBody));
}
 
// Target file: upload_store + file name of the URI if configured, otherwise root + URI
std::string ResponseBuilder::resolveDirectTargetPath() const {
	std::string targetFilePath = path; // Default: normal path (root + URI)
 
	if (!loc.uploadStore.empty()) {
		// Extract only the file name from the original URI (e.g. "/uploads/photo.png" -> "photo.png")
		std::string uri = req.getUri();
		std::string filename = "";
		std::string::size_type pos = uri.find_last_of('/');
		if (pos != std::string::npos) {
			filename = uri.substr(pos + 1);
		} else {
			filename = uri;
		}
 
		// Build the new path using the configured upload directory
		targetFilePath = loc.uploadStore;
		if (targetFilePath[targetFilePath.size() - 1] != '/') {
			targetFilePath += "/";
		}
		targetFilePath += filename;
	}
	return (targetFilePath);
}