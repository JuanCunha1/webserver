#include "Request.hpp"

Request() {
	this->method = "";
	this->uri = "";
	this->query = "";
	this->version = "";
	this->headers = "";
	this->body = "";

	this->isComplete = false;
	this->errorCode = 0;
}

Request(const Request &src) {
	*this = src;
}

Request &operator=(const Request &rhs) {
	if (this != &rhs) {
		this->method = rhs.method;
		this->uri = rhs.uri;
		this->query = rhs.query;
		this->version = rhs.version;
		this->headers = rhs.headers;
		this->body = rhs.body;
	}
	return (*this);
}

~Request() {

}