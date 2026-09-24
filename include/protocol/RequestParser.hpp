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
		size_t		_maxBodySize;
		std::string	_buffer;
		State 		_state;
		Request		req;
		
		RequestParser();
		
		

		bool parseLine(std::string &line);
		bool parseRequestLine(const std::string &line);
		bool parseHeaderLine(const std::string &line);
		std::string trim(const std::string &str);
		void processChunked();
		
		void validateHost(const Request &req);
		unsigned long parseContentLength(const std::string &value);
		void setupBodyParsing();

		void checkInitialEmptyLines();
		void processRequestLineState();
		void processHeadersState();
		void processContentLengthState();

		//* Para parseRequestLine
		void validateMethod(const std::string &method);
		void validateVersion(const std::string &version);
		void parseUri(const std::string &rawUri);
		
		//* Para parseHeaderLine
		bool handleDuplicateHeader(const std::string &key, const std::string &value);

		//* Para processChunked
		void processChunkSizeState();
		void processChunkDataState();
		void processChunkTrailerState();

	public:
		RequestParser(size_t maxBodySize);
		RequestParser(const RequestParser &src);
		RequestParser &operator=(const RequestParser &rhs);
		~RequestParser();

		void append(const std::string &data);
		void process();
		Request &getRequest();
		State getState() const;
};