#include "../../include/protocol/Request.hpp"
Request::Request() :
	method(""),
	uri(""),
	query(""),
	version(""),
	body(""),
	chunkSize(0),
	isComplete(false),
	errorCode(0) {
}

Request::Request(const Request &src) {
	*this = src;
}

Request &Request::operator=(const Request &rhs) {
	if (this != &rhs) {
		this->method = rhs.method;
		this->uri = rhs.uri;
		this->query = rhs.query;
		this->version = rhs.version;
		this->headers = rhs.headers;
		this->body = rhs.body;
		this->chunkSize = rhs.chunkSize;
		this->isComplete = rhs.isComplete;
        this->errorCode = rhs.errorCode;
	}
	return (*this);
}

Request::~Request() {
}