#include "../../../include/protocol/ResponseBuilder.hpp"
#include "../../../include/Utils.hpp"
#include <sstream>
#include <fstream>

std::string ResponseBuilder::getDefaultStatusMessage(int statusCode) {
    switch (statusCode) {
        // 1xx: Informational
        case 100: return ("Continue");
        case 101: return ("Switching Protocols");

        // 2xx: Success
        case 200: return ("OK");
        case 201: return ("Created");
        case 202: return ("Accepted");
        case 203: return ("Non-Authoritative Information");
        case 204: return ("No Content");
        case 205: return ("Reset Content");
        case 206: return ("Partial Content");

        // 3xx: Redirection
        case 300: return ("Multiple Choices");
        case 301: return ("Moved Permanently");
        case 302: return ("Found");
        case 303: return ("See Other");
        case 304: return ("Not Modified");
        case 305: return ("Use Proxy");
        case 307: return ("Temporary Redirect");
        case 308: return ("Permanent Redirect");

        // 4xx: Client Error
        case 400: return ("Bad Request");
        case 401: return ("Unauthorized");
        case 402: return ("Payment Required");
        case 403: return ("Forbidden");
        case 404: return ("Not Found");
        case 405: return ("Method Not Allowed");
        case 406: return ("Not Acceptable");
        case 407: return ("Proxy Authentication Required");
        case 408: return ("Request Timeout");
        case 409: return ("Conflict");
        case 410: return ("Gone");
        case 411: return ("Length Required");
        case 412: return ("Precondition Failed");
        case 413: return ("Payload Too Large"); // Antiguamente "Request Entity Too Large" en RFC 2616
        case 414: return ("URI Too Long");      // Antiguamente "Request-URI Too Long" en RFC 2616
        case 415: return ("Unsupported Media Type");
        case 416: return ("Range Not Satisfiable"); // Antiguamente "Requested Range Not Satisfiable"
        case 417: return ("Expectation Failed");
        case 418: return ("I'm a teapot");      // RFC 2324 / RFC 7168
        case 421: return ("Misdirected Request");
        case 422: return ("Unprocessable Content"); // RFC 4918 / RFC 9110
        case 423: return ("Locked");                // WebDAV / RFC 4918
        case 424: return ("Failed Dependency");     // WebDAV / RFC 4918
        case 426: return ("Upgrade Required");
        case 428: return ("Precondition Required"); // RFC 6585
        case 429: return ("Too Many Requests");     // RFC 6585
        case 431: return ("Request Header Fields Too Large"); // RFC 6585
        case 451: return ("Unavailable For Legal Reasons");   // RFC 7725

        // 5xx: Server Error
        case 500: return ("Internal Server Error");
        case 501: return ("Not Implemented");
        case 502: return ("Bad Gateway");
        case 503: return ("Service Unavailable");
        case 504: return ("Gateway Timeout");
        case 505: return ("HTTP Version Not Supported");
        case 506: return ("Variant Also Negotiates"); // RFC 2295
        case 507: return ("Insufficient Storage");   // WebDAV / RFC 4918
        case 508: return ("Loop Detected");          // WebDAV / RFC 5842
        case 510: return ("Not Extended");           // RFC 2774
        case 511: return ("Network Authentication Required"); // RFC 6585

        default:
            return ("Unknown Status");
    }
}

std::string ResponseBuilder::getDefaultErrorPage(int errorCode, const std::string& statusMsg) {
    std::ostringstream oss;
    oss << "<html>\r\n"
        << "<head><title>" << errorCode << " " << statusMsg << "</title></head>\r\n"
        << "<body style=\"font-family: monospace; text-align: center; margin-top: 50px;\">\r\n"
        << "<h1>" << errorCode << " " << statusMsg << "</h1>\r\n"
        << "<hr><p>webserv/1.0</p>\r\n"
        << "</body>\r\n"
        << "</html>\r\n";
    return oss.str();
}

Response ResponseBuilder::buildErrorResponse(int code, const std::string &msg) {
    Response res;
    std::string defaultBody = getDefaultErrorPage(code, msg);

    res.setStatusCode(code);
    res.setStatusMessage(msg);
    res.setHeader("Content-Type", "text/html");
    res.setHeader("Content-Length", Utils::toString(defaultBody.size()));
    
    res.setHeader("Connection", "close"); 
    res.setBody(defaultBody);

    return res;
}

//* Busca si existe página de error personalizada en struct ErrorPages
std::string ResponseBuilder::getCustomErrorPage(int errorCode) {
	//std::cout << "Error code: " << errorCode << std::endl;
	//std::cout << "Server error codes: " << server.errorPages[0].errorCodes[0] << std::endl;
	//std::cout << "Server error path: " << server.errorPages[0].errorPath << std::endl;
    for (size_t i = 0; i < server.errorPages.size(); ++i) {
        for (size_t j = 0; j < server.errorPages[i].errorCodes.size(); ++j) {
            if (server.errorPages[i].errorCodes[j] == errorCode) {
                std::string errorFilePath = server.root; 
                
                if (!errorFilePath.empty() && errorFilePath[errorFilePath.length()-1] != '/' 
                    && server.errorPages[i].errorPath[0] != '/') {
                    errorFilePath += "/";
                }
                errorFilePath += server.errorPages[i].errorPath;
                std::ifstream file(errorFilePath.c_str());
                if (file.is_open()) {
                    std::stringstream buffer;
                    buffer << file.rdbuf();
                    return buffer.str(); 
                } else {
                    std::cerr << "Webserv Warning: No se pudo abrir error_page custom: " << errorFilePath << std::endl;
                }
                return ""; 
            }
        }
    }
    return "";
}

Response ResponseBuilder::handleError(int errorCode) {
    std::string statusMsg = getDefaultStatusMessage(errorCode);
    std::cout << "Generating error: " << errorCode << std::endl;
    
    std::string customBody = getCustomErrorPage(errorCode);

    if (customBody.empty()) {
        Response res = buildErrorResponse(errorCode, statusMsg);
        if (!shouldCloseConnection(errorCode)) {
            res.setHeader("Connection", "keep-alive");
        }
        return res;
    }

    Response res;
    res.setStatusCode(errorCode);
    res.setStatusMessage(statusMsg);
    res.setHeader("Content-Type", "text/html");
    res.setHeader("Content-Length", Utils::toString(customBody.size()));
    res.setHeader("Connection", shouldCloseConnection(errorCode) ? "close" : "keep-alive");
    res.setBody(customBody);
    
    return res;
}