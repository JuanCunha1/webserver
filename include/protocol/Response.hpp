#pragma once

#include <string>
#include <map>
#include <iostream>

class Response {
	private:
		int _statusCode;
		std::string _statusMessage;
		std::map<std::string, std::string> _headers;
		std::string _body;

	public:
		Response();
		~Response();

		void setStatusCode(int code);
		void setStatusMessage(const std::string &msg);
		void setHeader(const std::string &key, const std::string &value);
		void setBody(const std::string &body);

		int getStatusCode() const { return (this->_statusCode); }
		const std::string &getStatusMessage() const { return (this->_statusMessage); }
		const std::string &getBody() const { return (this->_body); }
		
		//* Lo transforma en una string para poder pasarselo al socket
		std::string &serialize() const;

		//! TESTING
		void printHeadersRes() const {
			std::map<std::string, std::string>::const_iterator it = this->_headers.begin();
			while (it != this->_headers.end()) {
				//printar
				std::cout << it->first << ": " << it->second << std::endl;
				it++;
			}
		}

};