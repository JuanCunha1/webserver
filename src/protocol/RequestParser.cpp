#include "protocol/RequestParser.hpp"
#include "protocol/HttpException.hpp"
#include "Utils.hpp"

#include <cerrno>

static bool isValidHeaderName(const std::string &name);

RequestParser::RequestParser() {
}
/*
//! Creo que no hace falta porque no hay variables
RequestParser::RequestParser(const RequestParser &src) {
	*this = src;
}

RequestParser &RequestParser::operator=(const RequestParser &rhs) {
	if (this != &rhs) {
		// Empty
	}
	return (*this);
}
*/
RequestParser::RequestParser(size_t maxBodySize) {
	_maxBodySize = maxBodySize 
	_state = REQUEST_LINE;
	_buffer = NULL;
}

RequestParser::~RequestParser() {
}

void	RequestParser::setBuffer(std::string &buffer) {
	_buffer = buffer; 
}

//! RFC allows empty lines at first, so we ignore them and wait for more lines
void RequestParser::process(Request &req, State &state, size_t maxBodySize) {
	try {
		//* Empty/clrn/rn check
		checkInitialEmptyLines(_buffer);

		//* Orquestador de la máquina de estados
		while (state != COMPLETE && state != ERROR) {
			State prevState = state;

			if (state == REQUEST_LINE) {
				processRequestLineState(req, state);
			} 
			else if (state == HEADERS) {
				processHeadersState(req, state, maxBodySize);
			} 
			else if (state == BODY_CONTENT_LENGTH) {
				processContentLengthState(req, state);
			} 
			else if (state == CHUNK_SIZE || state == CHUNK_DATA || state == CHUNK_TRAILER) {
				processChunked(req, state, maxBodySize);
			}

			// Si el estado no ha cambiado, necesitamos leer más datos del socket
			if (state == prevState) {
				break;
			}
		}
	} catch (const HttpException &e) {
		state = ERROR;
		throw; // Relanzamos hacia el bloque de respuesta
	}
}

void RequestParser::checkInitialEmptyLines() {
	size_t start = 0;
	while (start < _buffer.length() && (_buffer[start] == '\r' || _buffer[start] == '\n')) {
		start++;
	}
	if (start == _buffer.length()) {
		throw HttpException(400);
	}
}

void RequestParser::processRequestLineState(Request &req, State &state) {
	std::string line;
	while (state == REQUEST_LINE) {
		if (!parseLine(line, state)) {
			//std::cout << "BUFFER1: " << buffer << std::endl;
			return;
		}
		//std::cout << "BUFFER2: " << line << std::endl;
		if (line.empty())
			continue;

		if (!parseRequestLine(line, req, state)) {
			state = ERROR;
			return;
		}
		state = HEADERS;
	}
}

void RequestParser::processHeadersState(Request &req, State &state, size_t maxBodySize) {
	std::string line;
	while (parseLine(line, state)) {
		if (line.empty()) {
			//* Host validation
			if (req.version == "HTTP/1.1") {
				validateHost(req);
			}
			
			//* Comprobaciones de Transfer-Encoding y Content-Length
			setupBodyParsing(req, state, maxBodySize);
			break;
		}
		
		if (!parseHeaderLine(line, req, state)) {
			state = ERROR;
			return;
		}
	}
}

void RequestParser::processContentLengthState(Request &req, State &state) {
	const std::string *contentLength = req.getHeader("content-length");
	size_t expectedLength = std::strtoul(contentLength->c_str(), NULL, 10);

	if (_buffer.size() >= expectedLength) {
		req.body = _buffer.substr(0, expectedLength);
		_buffer.erase(0, expectedLength);
		state = COMPLETE;
		req.isComplete = true;
	}
}

void RequestParser::processChunked(Request &req, State &state, size_t maxBodySize) {
	try {
		while (true) {
			State prevState = state;

			if (state == CHUNK_SIZE) {
				processChunkSizeState(req, state, maxBodySize);
			}
			else if (state == CHUNK_DATA) {
				processChunkDataState(req, state);
			}
			else if (state == CHUNK_TRAILER) {
				processChunkTrailerState(req, state);
			}

			// Salimos si el estado es final o si no ha cambiado (falta leer más del buffer)
			if (state == COMPLETE || state == ERROR || state == prevState) {
				break;
			}
		}
	} catch (const HttpException &e) {
		state = ERROR;
		throw;
	}
}

bool RequestParser::parseRequestLine(const std::string &line, Request &req, State &state) {
	std::istringstream iss(line);
	std::string method;
	std::string uri;
	std::string version;
	std::string extra;

	if (!(iss >> method >> uri >> version) || (iss >> extra)) {
		state = ERROR;
		throw HttpException(400);
	}

	try {
		validateMethod(method);
		validateVersion(version);
		parseUri(uri, req);
	} catch (const HttpException &e) {
		state = ERROR;
		throw;
	}

	req.method = method;
	req.version = version;

	return (true);
}

bool RequestParser::parseHeaderLine(const std::string &line, Request &req, State &state) {
	size_t colonPos = line.find(':');
	if (colonPos == std::string::npos) {
		state = ERROR;
		throw HttpException(400);
	}
	//! HTTP (RFC 7230 / RFC 9112) standard check
	//* Sin key, espacio previo, tab previo
	if (colonPos == 0 || line[colonPos - 1] == ' ' || line[colonPos - 1] == '\t') {
		state = ERROR;
		throw HttpException(400);
	}
	std::string key = line.substr(0, colonPos);
	std::string value = line.substr(colonPos + 1);
	
	// Nota: Si declaras isValidHeaderName en el hpp o en Utils, inclúyela aquí. 
	// Asumo que la mantienes como función estática separada o en Utils.
	if (!isValidHeaderName(key)) {
		state = ERROR;
		throw HttpException(400);
	}

	size_t i = 0;
	while (i < key.length()) {
		key[i] = std::tolower(static_cast<unsigned char>(key[i]));
		++i;
	}

	try {
		if (handleDuplicateHeader(key, trim(value), req)) {
			return (true);
		}
	} catch (const HttpException &e) {
		state = ERROR;
		throw;
	}

	req.headers[key] = trim(value);

	return (true);
}

void RequestParser::processChunkSizeState(Request &req, State &state, size_t maxBodySize) {
	std::string line;
	if (!parseLine(line, state))
		return;
	//* Eliminate Chunk Extensions (Nginx ignore it)
	size_t semiPos = line.find(';');
	if (semiPos != std::string::npos)
		line = line.substr(0, semiPos);
	//* Clean line and check if is empty (syntax error)
	line = trim(line);
	if (line.empty()) {
		throw HttpException(400);
	}
	//* Hexadecimal conversion and check if is good
	char *endPtr = NULL;
	errno = 0;
	size_t size = std::strtoul(line.c_str(), &endPtr, 16);
	if (*endPtr != '\0' || errno != 0) {
		throw HttpException(400);
	}

	req.chunkSize = size;
	//* Check max body server size
	if (maxBodySize > 0 && (req.getBody().size() + size > maxBodySize)) {
		throw HttpException(413);
	}

	if (size == 0)
		state = CHUNK_TRAILER;
	else
		state = CHUNK_DATA;
}

void RequestParser::processChunkDataState(Request &req, State &state) {
	size_t size = req.getChunkSize();

	if (_buffer.size() < size + 2)
		return;

	if (_buffer.substr(size, 2) != "\r\n") {
		throw HttpException(400);
	}
	req.body.append(_buffer, 0, size);
	_buffer.erase(0, size + 2);

	state = CHUNK_SIZE;
}

void RequestParser::processChunkTrailerState(Request &req, State &state) {
	std::string line;
	if (!parseLine(line, state))
		return;

	if (line.empty()) {
		state = COMPLETE;
		req.isComplete = true;
		return;
	}
}

void RequestParser::validateMethod(const std::string &method) {
	//! Comprobaciones method
	if (!Utils::isAllUpper(method)) {
		throw HttpException(400);
	}
	//* Para evitar errores en testers si no es passan cabeceras
	//* Porque como hacemos la comprobación en la respuesta salta otro error antes...
	if (method != "GET" && method != "POST" && method != "DELETE") {
		throw HttpException(501); // 501 Not Implemented
	}
}

void RequestParser::validateVersion(const std::string &version) {
	//! Comprobaciones version
	if (version.size() < 5 || version.compare(0, 5, "HTTP/") != 0) {
		throw HttpException(400);
	}
	if (version != "HTTP/1.1" && version != "HTTP/1.0") {
		throw HttpException(505);
	}
}

void RequestParser::parseUri(const std::string &rawUri, Request &req) {
	//! Comprobaciones uri
	if (rawUri.compare(0, 1, "/") != 0) {
		throw HttpException(400);
	}
	if (rawUri.size() > 2048) {
		throw HttpException(414);
	}
	//* Separate uri and query
	size_t queryPos = rawUri.find('?');
	if (queryPos != std::string::npos) {
		req.uri = rawUri.substr(0, queryPos);
		req.query = rawUri.substr(queryPos + 1);
	} else {
		req.uri = rawUri;
		req.query = "";
	}
}

void RequestParser::validateHost(const Request &req) {
	const std::string *host = req.getHeader("host");
	if (!host || host->empty()) {
		throw HttpException(400);
	}
}

void RequestParser::setupBodyParsing(Request &req, State &state, size_t maxBodySize) {
	const std::string *transferEncoding = req.getHeader("transfer-encoding");
	const std::string *contentLength = req.getHeader("content-length");

	//* Forbidden Content-Length & Transfer-Encoding at the same request
	if (transferEncoding && contentLength) {
		throw HttpException(400);
	}
	//* Transfer encoding check
	if (transferEncoding) {
		if (*transferEncoding == "chunked") {
			state = CHUNK_SIZE;
		} else {
			throw HttpException(501);
		}
	}
	//* Content-Length check
	else if (contentLength) {
		unsigned long expectedLen = parseContentLength(*contentLength);

		if (maxBodySize > 0 && expectedLen > maxBodySize) {
			throw HttpException(413);
		}

		if (expectedLen == 0) {
			state = COMPLETE;
			req.isComplete = true;
		} else {
			state = BODY_CONTENT_LENGTH;
		}
	}
	//* POST always need body
	else if (req.method == "POST") {
		throw HttpException(411);
	}
	else {
		state = COMPLETE;
		req.isComplete = true;
	}
}

unsigned long RequestParser::parseContentLength(const std::string &value) {
	if (value.empty()) {
		throw HttpException(400);
	}
	
	size_t j = 0;
	while (j < value.length()) {
		if (!std::isdigit(static_cast<unsigned char>(value[j]))) {
			throw HttpException(400);
		}
		++j;
	}

	char *endPtr = NULL;
	errno = 0;
	unsigned long expectedLen = std::strtoul(value.c_str(), &endPtr, 10);
	if (*endPtr != '\0' || errno != 0) {
		throw HttpException(400);
	}
	
	return (expectedLen);
}

bool RequestParser::handleDuplicateHeader(const std::string &key, const std::string &value, Request &req) {
	// #15 y #23: Manejo de cabeceras duplicadas
	if (req.headers.find(key) != req.headers.end()) {
		//* more than one HOST header
		if (key == "host") {
			throw HttpException(400);
		}
		//* if content-length is duplicated with diferent values
		if (key == "content-length") {
			if (req.headers[key] != value) {
				throw HttpException(400);
			}
			return (true); // Si es idéntico se puede tolerar y saltamos la asignación final
		}
	}
	return (false);
}

bool RequestParser::parseLine(std::string &line, State &state) {
	size_t lf_pos = _buffer.find('\n');
	if (lf_pos == std::string::npos) {
		return (false);
	}
	if (lf_pos == 0 || _buffer[lf_pos - 1] != '\r') {
		state = ERROR;
		throw HttpException(400); // Salto de línea inválido (solo LF) -> 400 Bad Request
	}

	line = _buffer.substr(0, lf_pos - 1);
	_buffer.erase(0, lf_pos + 1);
	return (true);
}

static bool isValidHeaderName(const std::string &name) {
    if (name.empty()) return false;
    const std::string separators = "()<>@,;:\\\"/[]?={} \t";
    size_t i = 0;
	while (i < name.length()) {
        unsigned char c = name[i];
        if (c <= 32 || c >= 127 || separators.find(c) != std::string::npos)
            return (false);
		++i;
    }
    return (true);
}

std::string RequestParser::trim(const std::string &str) {
	size_t first = str.find_first_not_of(" \t\r\n");
	if (first == std::string::npos) {
		return ("");
	}
	size_t last = str.find_last_not_of(" \t\r\n");
	return (str.substr(first, (last - first + 1)));
}