#pragma once

#include "Request.hpp"
#include "Response.hpp"
#include "config/ConfigServer.hpp"
#include "CgiHandler.hpp"
#include <vector>

struct HandlerResult {
    bool isCgi;
    Response staticResponse;
    CgiHandler *cgiHandler;

    HandlerResult() : isCgi(false), cgiHandler(NULL) {}
};

class ResponseBuilder {
	private:
		Request			req;
		ConfigLocation	loc;
		ConfigServer	server;
		std::string		path;

		HandlerResult handleGet();
		HandlerResult handlePost();
		HandlerResult handleDelete();
		Response serveStaticFile(const std::string &filePath);
		
		bool shouldCloseConnection(int statusCode);
		Response handlePostDirect();

		bool	isCgiRequest();
		bool	isCgiExtension(const std::string& ext, std::string& outCgiBinary);
		
		std::string normalizeUri(const std::string& uri);

		std::string getCustomErrorPage(int errorCode);
		static std::string getDefaultErrorPage(int errorCode, const std::string& statusMsg);

	public:
		ResponseBuilder(const Request &request, const ConfigLocation &location, const ConfigServer &server);
		~ResponseBuilder();
		//! Para poderlo usar en el main lo pongo en public
		Response handleError(int errorCode);

		static std::string getDefaultStatusMessage(int statusCode);

		//! Para poder usar en server
		static Response buildErrorResponse(int code, const std::string &msg);
		//* Esta función se va a encargar de montar la respuesta
		HandlerResult buildResponse();
};