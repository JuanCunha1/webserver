#pragma once

#include <string>
#include <map>
#include <iostream>

class RequestParser;

class Request {
	private:
		std::string method;
		std::string uri;
		std::string query;
		std::string version;
		std::map<std::string, std::string> headers;
		std::string body;
		size_t chunkSize;

		bool isComplete;
		int errorCode;

		friend class RequestParser;

	public:
		Request();
		Request(const Request &src);
		Request &operator=(const Request &rhs);
		~Request();

		const std::string &getMethod() const { return (this->method); }
		const std::string &getUri() const { return (this->uri); }
		const std::string &getQuery() const { return (this->query); }
		const std::string &getVersion() const { return (this->version); }
		const std::string &getBody() const { return (this->body); }
		size_t getChunkSize() const { return (this->chunkSize); }

		const std::string *getHeader(const std::string &key) const {
			std::map<std::string, std::string>::const_iterator it = this->headers.find(key);
			if (it != this->headers.end()) {
				return &(it->second);
			}
			return (NULL);
		}
		//! Mas que nada para ver como implementar de manera adequada las excepciones
		/*
		const std::string& getHeader(const std::string& key) const {
		std::map<std::string, std::string>::const_iterator it = this->headers.find(key);
		if (it != this->headers.end()) {
			return it->second;
		}
		throw std::out_of_range("Header no encontrado: " + key);
	}
		*/
		

		bool getIsComplete() const { return this->isComplete; }
        int getErrorCode() const { return this->errorCode; }

		//! TESTING
		void printHeaders() const {
			std::map<std::string, std::string>::const_iterator it = this->headers.begin();
			while (it != this->headers.end()) {
				//printar
				std::cout << it->first << ": " << it->second << std::endl;
				it++;
			}
		}

};