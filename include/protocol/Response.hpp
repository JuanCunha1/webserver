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
		
		//* Lo transforma en una string para poder pasarselo al socket
		//! al momento de llamar a Response::serialize() o Response::toString(),
		//! lanzas una excepción o devuelves un 500 Internal Server Error si el código sigue siendo 0.
		//std::string serialize() const;
		//* El body lo pasamos a send con getBody para evitar hacer una copia
		//* Es un bloque contiguo en memoria así que podemos passar el puntero al bloque
		std::string getHeadersAsString() const;


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