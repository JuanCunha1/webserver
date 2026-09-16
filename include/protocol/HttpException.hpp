#pragma once

#include <exception>
#include <string>

class HttpException : public std::exception {
private:
    int         _statusCode;
    std::string _statusMessage;

    static std::string getDefaultStatusMessageExc(int code) {
        switch (code) {
            case 200: return "OK";
            case 201: return "Created";
            case 204: return "No Content";
            case 400: return "Bad Request";
            case 403: return "Forbidden";
            case 404: return "Not Found";
            case 405: return "Method Not Allowed";
            case 411: return "Length Required";
            case 413: return "Payload Too Large";
            case 414: return "URI Too Long";
            case 500: return "Internal Server Error";
            case 501: return "Not Implemented";
            case 505: return "HTTP Version Not Supported";
            default:  return "Unknown Status Code";
        }
    }

public:
    // Si no le pasas mensaje, usa el predeterminado por el estándar
    explicit HttpException(int code, const std::string &detail = "")
        : _statusCode(code) {
        if (detail.empty()) {
            _statusMessage = getDefaultStatusMessageExc(code);
        } else {
            _statusMessage = detail;
        }
    }

    virtual ~HttpException() throw() {}

    virtual const char* what() const throw() {
        return (_statusMessage.c_str());
    }

    int getStatusCode() const throw() {
        return (_statusCode);
    }

    const std::string& getStatusMessage() const throw() {
        return (_statusMessage);
    }
};