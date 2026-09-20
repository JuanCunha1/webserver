#pragma once

#include "Request.hpp"
#include "Response.hpp"
#include "../config/ConfigServer.hpp"

class ResponseBuilder {
	private:
		static Response handleGet(const Request &req, const std::string &path, const ConfigLocation &loc);
		static Response handlePost(const Request &req, const std::string &path, const ConfigLocation &loc);
		static Response handleDelete(const Request &req, const std::string &path);
		static Response serveStaticFile(const Request &req, const std::string &filePath);
		static Response buildErrorResponse(int code, const std::string &msg);
		static bool shouldCloseConnection(const Request &req, int statusCode);

		static Response handlePostDirect(const Request &req, const std::string &path);

		static bool	isCgiRequest(const std::string &path, const ConfigLocation &loc);
		static std::string getCgiBinary(const std::string &path, const ConfigLocation &loc);

	public:
		//! Para poderlo usar en el main lo pongo en public
		static Response handleError(const Request &req, int errorCode);
		//* Esta función se va a encargar de montar la respuesta
		static Response buildResponse(const Request &req, const std::string &path, const ConfigLocation &loc);
};