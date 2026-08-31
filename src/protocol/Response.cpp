#include "../../include/protocol/Response.hpp"

Response::Response() :
	_statusCode(0),
	_statusMessage(""),
	_body("") {	
}

Response::~Response() {
}

void Response::setStatusCode(int code) {
	this->_statusCode = code;
}

void Response::setStatusMessage(const std::string &msg) {
	this->_statusMessage = msg;
}

void Response::setHeader(const std::string &key, const std::string &value) {
	this->_headers[key] = value;
}

void Response::setBody(const std::string &body) {
	this->_body = body;
}