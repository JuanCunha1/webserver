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
		std::string		path;

		HandlerResult handleGet();
		HandlerResult handlePost();
		HandlerResult handleDelete();
		Response serveStaticFile(const std::string &filePath);
		
		bool shouldCloseConnection(int statusCode);
		Response handlePostDirect();

		bool	isCgiRequest();
		std::string getCgiBinary();
		
		std::string normalizeUri(const std::string& uri);

	public:
		ResponseBuilder(const Request &request, const ConfigLocation &location);
		~ResponseBuilder();
		//! Para poderlo usar en el main lo pongo en public
		Response handleError(int errorCode);
		//! Para poder usar en server
		static Response buildErrorResponse(int code, const std::string &msg);
		//* Esta función se va a encargar de montar la respuesta
		HandlerResult buildResponse();
};