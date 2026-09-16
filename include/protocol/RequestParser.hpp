#pragma once

#include <string>
#include <sstream>
#include <cstdlib>
#include <cctype>
#include <map>

#include "Request.hpp"

enum ParsingState {
    STATE_REQUEST_LINE,
    STATE_HEADERS,
    STATE_BODY_CONTENT_LENGTH,
    STATE_CHUNK_SIZE,
    STATE_CHUNK_DATA,
    STATE_CHUNK_TRAILER,
    STATE_COMPLETE,
    STATE_ERROR
};

class RequestParser {
	private:
		static bool parseLine(std::string &buffer, std::string &line);
		static bool parseRequestLine(const std::string &line, Request &req);
        static bool parseHeaderLine(const std::string &line, Request &req);
        static std::string trim(const std::string &str);
		static void processChunked(std::string &buffer, Request &req, ParsingState &state, size_t maxBodySize);

	public:
		RequestParser();
		//RequestParser(const RequestParser &src);
		//RequestParser &operator=(const RequestParser &rhs);
		~RequestParser();

		//* Parse from client buffer and modify buffer eliminating de processed part
		//* maxBodySize proceeds from .conf of server ()
		static void process(std::string &buffer, Request &req, ParsingState &state, size_t maxBodySize);
};