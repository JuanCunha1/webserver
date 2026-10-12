#include "protocol/Response.hpp"
#include "protocol/Request.hpp"
#include "protocol/HttpException.hpp"
#include "Utils.hpp"

Response::Response() :
	//! Hardcoding the version is not the right thing
	_version("HTTP/1.1"),
	_statusCode(0),
	_statusMessage(""),
	_body("") {	

	setHeader("Date", Utils::getCurrentDateGMT());
}

void Response::setVersion(const std::string &version) {
	this->_version = version;
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

// Helper function in C++98 for int to string
static std::string intToString(int n) {
    std::ostringstream ss;
    ss << n;
    return (ss.str());
}

std::string Response::getHeadersAsString() const {
    std::string h;
	if (this->_statusCode == 0) {
		throw HttpException(500);
	}
    h.reserve(512);

    h += _version + " " + intToString(_statusCode) + " " + _statusMessage + "\r\n";

	std::map<std::string, std::string>::const_iterator it = _headers.begin();
    while (it != _headers.end()) {
        h += it->first + ": " + it->second + "\r\n";
		++it;
    }
    h += "\r\n";
    return (h);
}