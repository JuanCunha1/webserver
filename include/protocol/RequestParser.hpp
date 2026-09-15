#pragma once

#include <string>
#include <sstream>
#include <cstdlib>
#include <cctype>
#include <map>

#include "Request.hpp"

class RequestParser {
	public:
		enum State {
			REQUEST_LINE,
			HEADERS,
			BODY_CONTENT_LENGTH,
			CHUNK_SIZE,
			CHUNK_DATA,
			CHUNK_TRAILER,
			COMPLETE,
			ERROR
		};

	private:
		RequestParser();
		//RequestParser(const RequestParser &src);
		//RequestParser &operator=(const RequestParser &rhs);
		~RequestParser();

		static bool parseLine(std::string &buffer, std::string &line, State &state);
		static bool parseRequestLine(const std::string &line, Request &req, State &state);
		static bool parseHeaderLine(const std::string &line, Request &req, State &state);
		static std::string trim(const std::string &str);
		static void processChunked(std::string &buffer, Request &req, State &state, size_t maxBodySize);
		
		static void validateHost(const Request &req);
		static unsigned long parseContentLength(const std::string &value);
		static void setupBodyParsing(Request &req, State &state, size_t maxBodySize);

		static void checkInitialEmptyLines(const std::string &buffer);
		static void processRequestLineState(std::string &buffer, Request &req, State &state);
		static void processHeadersState(std::string &buffer, Request &req, State &state, size_t maxBodySize);
		static void processContentLengthState(std::string &buffer, Request &req, State &state);

		// --- Para parseRequestLine ---
		static void validateMethod(const std::string &method);
		static void validateVersion(const std::string &version);
		static void parseUri(const std::string &rawUri, Request &req);
		
		// --- Para parseHeaderLine ---
		static bool handleDuplicateHeader(const std::string &key, const std::string &value, Request &req);

		// --- Para processChunked ---
		static void processChunkSizeState(std::string &buffer, Request &req, State &state, size_t maxBodySize);
		static void processChunkDataState(std::string &buffer, Request &req, State &state);
		static void processChunkTrailerState(std::string &buffer, Request &req, State &state);

	public:
		//* Parse from client buffer and modify buffer eliminating de processed part
		//* maxBodySize proceeds from .conf of server ()
		static void process(std::string &buffer, Request &req, State &state, size_t maxBodySize);
};