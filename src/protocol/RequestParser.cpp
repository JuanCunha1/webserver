#include "../../include/protocol/RequestParser.hpp"

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

RequestParser::~RequestParser() {
}

std::string RequestParser::trim(const std::string &str) {
	size_t first = str.find_first_not_of(" \t\r\n");
	if (first == std::string::npos) {
		return ("");
	}
	size_t last = str.find_last_not_of(" \t\r\n");
	return (str.substr(first, (last - first + 1)));
}

bool RequestParser::parseLine(std::string &buffer, std::string &line) {
	size_t pos = buffer.find("\r\n");
	if (pos == std::string::npos)
		return false;
	line = buffer.substr(0, pos);
	buffer.erase(0, pos + 2);
	return (true);
}

bool RequestParser::parseRequestLine(const std::string &line, Request &req) {
	std::istringstream iss(line);
	std::string method;
	std::string uri;
	std::string version;
	std::string extra;

	if (!(iss >> method >> uri >> version) || (iss >> extra)) {
		req.errorCode = 400;
		return (false);
	}

	//! Check the version that we will use
	if (version != "HTTP/1.1" && version != "HTTP/1.0") {
        req.errorCode = 505;
        return false;
    }

	size_t queryPos = uri.find('?');
	if (queryPos != std::string::npos) {
		req.uri = uri.substr(0, queryPos);
		req.query = uri.substr(queryPos + 1);
	} else {
		req.uri = uri;
		req.query = "";
	}
	req.method = method;
	req.version = version;
	return (true);
}

bool RequestParser::parseHeaderLine(const std::string &line, Request &req) {
	size_t colonPos = line.find(':');
	if (colonPos == std::string::npos) {
		req.errorCode = 400;
		return (false);
	}
	//! HTTP (RFC 7230 / RFC 9112) standard check
	if (colonPos == 0 || line[colonPos - 1] == ' ' || line[colonPos - 1] == '\t') {
		req.errorCode = 400;
		return (false);
	}
	
	std::string key = line.substr(0, colonPos);
	std::string value = line.substr(colonPos + 1);

	size_t i = 0;
	while (i < key.length()) {
		//! HTTP (RFC 7230 / RFC 9112) standard check
		//* Empty space is not allowed before colon
		if (key[i] == ' ' || key[i] == '\t') {
			req.errorCode = 400;
			return (false);
		}
		key[i] = std::tolower(static_cast<unsigned char>(key[i]));
		i++;
	}
	req.headers[key] = trim(value);

	return (true);
}

void RequestParser::processChunked(std::string &buffer, Request &req, ParsingState &state, size_t maxBodySize) {
    while (true) {
        if (state == STATE_CHUNK_SIZE) {
            std::string line;
            if (!parseLine(buffer, line))
                return;
			//* Eliminate Chunk Extensions (Nginx ignore it)
            size_t semiPos = line.find(';');
            if (semiPos != std::string::npos)
                line = line.substr(0, semiPos);
			//* Clean line and check if is empty (syntax error)
            line = trim(line);
            if (line.empty()) {
                req.errorCode = 400;
                state = STATE_ERROR;
                return;
            }
			//* Hexadecimal conversion and check if is good
            char *endPtr = NULL;
            errno = 0;
            size_t size = std::strtoul(line.c_str(), &endPtr, 16);
            if (*endPtr != '\0' || errno != 0) {
                req.errorCode = 400;
                state = STATE_ERROR;
                return;
            }

            req.chunkSize = size;
			//* Check max body server size
            if (maxBodySize > 0 && (req.getBody().size() + size > maxBodySize)) {
                req.errorCode = 413;
                state = STATE_ERROR;
                return;
            }

            if (size == 0)
                state = STATE_CHUNK_TRAILER;
            else
                state = STATE_CHUNK_DATA;
        }

        if (state == STATE_CHUNK_DATA) {
            size_t size = req.getChunkSize();

            if (buffer.size() < size + 2)
                return;

            if (buffer.substr(size, 2) != "\r\n") {
                req.errorCode = 400;
                state = STATE_ERROR;
                return;
            }
            req.body.append(buffer, 0, size);
            buffer.erase(0, size + 2);

            state = STATE_CHUNK_SIZE;
        }

        if (state == STATE_CHUNK_TRAILER) {
            std::string line;
            if (!parseLine(buffer, line))
                return;

            if (line.empty()) {
                state = STATE_COMPLETE;
                req.isComplete = true;
                return;
            }
        }
    }
}

//! RFC allows empty lines at first, so we ignore them and wait for more lines
void RequestParser::process(std::string &buffer, Request &req, ParsingState &state, size_t maxBodySize) {
    std::string line;

    //* Request line
    if (state == STATE_REQUEST_LINE) {
        while (state == STATE_REQUEST_LINE) {
            if (!parseLine(buffer, line))
                return;
            if (line.empty())
                continue;

            if (!parseRequestLine(line, req)) {
                state = STATE_ERROR;
                return;
            }
            state = STATE_HEADERS;
        }
    }
    //* Headers
    if (state == STATE_HEADERS) {
        while (parseLine(buffer, line)) {
            if (line.empty()) {
                const std::string *transferEncoding = req.getHeader("transfer-encoding");
                const std::string *contentLength = req.getHeader("content-length");

                if (transferEncoding && *transferEncoding == "chunked") {
                    state = STATE_CHUNK_SIZE;
                } else if (contentLength) {
                    char *endPtr = NULL;
                    errno = 0;
                    size_t expectedLen = std::strtoul(contentLength->c_str(), &endPtr, 10);
                    if (*endPtr != '\0' || errno != 0) {
                        req.errorCode = 400;
                        state = STATE_ERROR;
                        return;
                    }

                    if (maxBodySize > 0 && expectedLen > maxBodySize) {
                        req.errorCode = 413;
                        state = STATE_ERROR;
                        return;
                    }

                    if (expectedLen == 0) {
                        state = STATE_COMPLETE;
                        req.isComplete = true;
                    } else {
                        state = STATE_BODY_CONTENT_LENGTH;
                    }
                } else {
                    state = STATE_COMPLETE;
                    req.isComplete = true;
                }
                break;
            }

            if (!parseHeaderLine(line, req)) {
                state = STATE_ERROR;
                return;
            }
        }
    }
    //* Content-Length
    if (state == STATE_BODY_CONTENT_LENGTH) {
        const std::string *contentLength = req.getHeader("content-length");
        size_t expectedLength = std::strtoul(contentLength->c_str(), NULL, 10);

        if (buffer.size() >= expectedLength) {
            req.body = buffer.substr(0, expectedLength);
            buffer.erase(0, expectedLength);
            state = STATE_COMPLETE;
            req.isComplete = true;
        }
    }
    //* Transfer-Encoding: chunked
    if (state == STATE_CHUNK_SIZE || state == STATE_CHUNK_DATA || state == STATE_CHUNK_TRAILER) {
        processChunked(buffer, req, state, maxBodySize);
    }
}