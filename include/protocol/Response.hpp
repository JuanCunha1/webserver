#pragma once

#include <string>
#include <map>
#include <iostream>
#include <sstream>

class Response {
	private:
		std::string							_version;
		int									_statusCode;
		std::string							_statusMessage;
		std::map<std::string, std::string>	_headers;
		std::string							_body;

	public:
		Response();
		~Response();
		
		void setVersion(const std::string &version);
		void setStatusCode(int code);
		void setStatusMessage(const std::string &msg);
		void setHeader(const std::string &key, const std::string &value);
		void setBody(const std::string &body);

		const std::string &getVersion() const { return (this->_version); }
		int getStatusCode() const { return (this->_statusCode); }
		const std::string &getStatusMessage() const { return (this->_statusMessage); }
		const std::string &getBody() const { return (this->_body); }
		
		//* Transforms it into a string so it can be passed to the socket
		//! when calling Response::serialize() or Response::toString(),
		//! throw an exception or return a 500 Internal Server Error if the code is still 0.
		//std::string serialize() const;
		//* We pass the body to send with getBody to avoid making a copy
		//* It is a contiguous block in memory so we can pass the pointer to the block
		std::string getHeadersAsString() const;


		//! TESTING
		void printHeadersRes() const {
			std::map<std::string, std::string>::const_iterator it = this->_headers.begin();
			while (it != this->_headers.end()) {
				//print
				std::cout << it->first << ": " << it->second << std::endl;
				it++;
			}
		}

};