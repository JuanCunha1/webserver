#include "protocol/RequestParser.hpp"
#include "protocol/HttpException.hpp"
#include "Utils.hpp"

#include <cerrno>

static bool isValidHeaderName(const std::string &name);

RequestParser::RequestParser() {
	_maxBodySize = 0;
	_state = REQUEST_LINE;
	req = Request();
}

RequestParser::RequestParser(size_t maxBodySize) {
	_maxBodySize = maxBodySize;
	_state = REQUEST_LINE;
	req = Request();
}

RequestParser::RequestParser(const RequestParser &src) {
	*this = src;
}

RequestParser &RequestParser::operator=(const RequestParser &rhs) {
	if (this != &rhs) {
		_maxBodySize = rhs._maxBodySize;
		_buffer = rhs._buffer;
		_state = rhs._state;
		req = rhs.req;
	}
	return (*this);
}
RequestParser::~RequestParser() {
}

void RequestParser::append(const std::string &data) {
	_buffer.append(data);
}

//! RFC allows empty lines at first, so we ignore them and wait for more lines
void RequestParser::process() {
	try {
		//* Empty/clrn/rn check
		checkInitialEmptyLines();

		//* Orquestador de la máquina de estados
		while (_state != COMPLETE && _state != ERROR) {
			State prevState = _state;
			if (_state == REQUEST_LINE) {
				processRequestLineState();
			} 
			else if (_state == HEADERS) {
				processHeadersState();
			} 
			else if (_state == BODY_CONTENT_LENGTH) {
				processContentLengthState();
			} 
			else if (_state == CHUNK_SIZE || _state == CHUNK_DATA || _state == CHUNK_TRAILER) {
				processChunked();
			}

			// Si el estado no ha cambiado, necesitamos leer más datos del socket
			if (_state == prevState) {
				break;
			}
		}
	} catch (const HttpException &e) {
		_state = ERROR;
		throw; // Relanzamos hacia el bloque de respuesta
	}
}

void RequestParser::checkInitialEmptyLines() {
	size_t start = 0;
	size_t end = _buffer.length();
	while (start < end && (_buffer[start] == '\r' || _buffer[start] == '\n')) {
		start++;
	}
}

void RequestParser::processRequestLineState() {
	std::string line;
	while (_state == REQUEST_LINE) {
		if (!parseLine(line)) {
			//std::cout << "BUFFER1: " << buffer << std::endl;
			return;
		}
		//std::cout << "BUFFER2: " << line << std::endl;
		if (line.empty())
			continue;

		if (!parseRequestLine(line)) {
			_state = ERROR;
			return;
		}
		_state = HEADERS;
	}
}

void RequestParser::processHeadersState() {
	std::string line;
	while (parseLine(line)) {
		if (line.empty()) {
			//* Host validation
			if (req.version == "HTTP/1.1") {
				validateHost(req);
			}
			
			//* Comprobaciones de Transfer-Encoding y Content-Length
			setupBodyParsing();
			break;
		}
		
		if (!parseHeaderLine(line)) {
			_state = ERROR;
			return;
		}
	}
}

void RequestParser::processContentLengthState() {
	const std::string *contentLength = req.getHeader("content-length");
	size_t expectedLength = std::strtoul(contentLength->c_str(), NULL, 10);

	if (_buffer.size() >= expectedLength) {
		req.body = _buffer.substr(0, expectedLength);
		_buffer.erase(0, expectedLength);
		_state = COMPLETE;
		req.isComplete = true;
	}
}

void RequestParser::processChunked() {
	try {
		while (true) {
			State prevState = _state;

			if (_state == CHUNK_SIZE) {
				processChunkSizeState();
			}
			else if (_state == CHUNK_DATA) {
				processChunkDataState();
			}
			else if (_state == CHUNK_TRAILER) {
				processChunkTrailerState();
			}

			// Salimos si el estado es final o si no ha cambiado (falta leer más del buffer)
			if (_state == COMPLETE || _state == ERROR || _state == prevState) {
				break;
			}
		}
	} catch (const HttpException &e) {
		_state = ERROR;
		throw;
	}
}

bool RequestParser::parseRequestLine(const std::string &line) {
	std::istringstream iss(line);
	std::string method;
	std::string uri;
	std::string version;
	std::string extra;

	if (!(iss >> method >> uri >> version) || (iss >> extra)) {
		_state = ERROR;
		throw HttpException(400);
	}

	try {
		validateMethod(method);
		validateVersion(version);
		parseUri(uri);
	} catch (const HttpException &e) {
		_state = ERROR;
		throw;
	}

	req.method = method;
	req.version = version;

	return (true);
}

bool RequestParser::parseHeaderLine(const std::string &line) {
	size_t colonPos = line.find(':');
	if (colonPos == std::string::npos) {
		_state = ERROR;
		throw HttpException(400);
	}
	//! HTTP (RFC 7230 / RFC 9112) standard check
	//* Sin key, espacio previo, tab previo
	if (colonPos == 0 || line[colonPos - 1] == ' ' || line[colonPos - 1] == '\t') {
		_state = ERROR;
		throw HttpException(400);
	}
	std::string key = line.substr(0, colonPos);
	std::string value = line.substr(colonPos + 1);
	
	// Nota: Si declaras isValidHeaderName en el hpp o en Utils, inclúyela aquí. 
	// Asumo que la mantienes como función estática separada o en Utils.
	if (!isValidHeaderName(key)) {
		_state = ERROR;
		throw HttpException(400);
	}

	size_t i = 0;
	size_t len = key.length();
	while (i < len) {
		key[i] = std::tolower(static_cast<unsigned char>(key[i]));
		++i;
	}

	try {
		if (handleDuplicateHeader(key, trim(value))) {
			return (true);
		}
	} catch (const HttpException &e) {
		_state = ERROR;
		throw;
	}

	req.headers[key] = trim(value);

	return (true);
}

void RequestParser::processChunkSizeState() {
	std::string line;
	if (!parseLine(line))
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
	if (_maxBodySize > 0 && (req.getBody().size() + size > _maxBodySize)) {
		throw HttpException(413);
	}

	if (size == 0)
		_state = CHUNK_TRAILER;
	else
		_state = CHUNK_DATA;
}

void RequestParser::processChunkDataState() {
	size_t size = req.getChunkSize();

	if (_buffer.size() < size + 2)
		return;

	if (_buffer.substr(size, 2) != "\r\n") {
		throw HttpException(400);
	}
	req.body.append(_buffer, 0, size);
	_buffer.erase(0, size + 2);

	_state = CHUNK_SIZE;
}

void RequestParser::processChunkTrailerState() {
	std::string line;
	if (!parseLine(line))
		return;

	if (line.empty()) {
		_state = COMPLETE;
		req.isComplete = true;
		
		//*Sustituimos la cabecera para el CGI
		req.headers.erase("transfer-encoding");
		std::ostringstream oss;
		oss << req.body.size();		
		req.headers["content-length"] = oss.str();
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

void RequestParser::parseUri(const std::string &rawUri) {
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

void RequestParser::setupBodyParsing() {
	const std::string *transferEncoding = req.getHeader("transfer-encoding");
	const std::string *contentLength = req.getHeader("content-length");

	//* Forbidden Content-Length & Transfer-Encoding at the same request
	if (transferEncoding && contentLength) {
		throw HttpException(400);
	}
	//* Transfer encoding check
	if (transferEncoding) {
		if (*transferEncoding == "chunked") {
			_state = CHUNK_SIZE;
		} else {
			throw HttpException(501);
		}
	}
	//* Content-Length check
	else if (contentLength) {
		unsigned long expectedLen = parseContentLength(*contentLength);

		if (_maxBodySize > 0 && expectedLen > _maxBodySize) {
			throw HttpException(413);
		}

		if (expectedLen == 0) {
			_state = COMPLETE;
			req.isComplete = true;
		} else {
			_state = BODY_CONTENT_LENGTH;
		}
	}
	//* POST always need body
	else if (req.method == "POST") {
		throw HttpException(411);
	}
	else {
		_state = COMPLETE;
		req.isComplete = true;
	}
}

unsigned long RequestParser::parseContentLength(const std::string &value) {
	if (value.empty()) {
		throw HttpException(400);
	}
	
	size_t j = 0;
	size_t len = value.length();
	while (j < len) {
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

bool RequestParser::handleDuplicateHeader(const std::string &key, const std::string &value) {
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

bool RequestParser::parseLine(std::string &line) {
	size_t lf_pos = _buffer.find('\n');
	if (lf_pos == std::string::npos) {
		return (false);
	}
	if (lf_pos == 0 || _buffer[lf_pos - 1] != '\r') {
		_state = ERROR;
		throw HttpException(400); // Salto de línea inválido (solo LF) -> 400 Bad Request
	}

	line = _buffer.substr(0, lf_pos - 1);
	_buffer.erase(0, lf_pos + 1);
	return (true);
}

static bool isValidHeaderName(const std::string &name) {
    if (name.empty()) 
		return false;
    const std::string separators = "()<>@,;:\\\"/[]?={} \t";
    size_t i = 0;
	size_t len = name.length();
	while (i < len) {
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
RequestParser::State RequestParser::getState() const {
	return _state;
}
