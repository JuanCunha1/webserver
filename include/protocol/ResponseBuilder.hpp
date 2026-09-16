#pragma once

#include "Request.hpp"
#include "Response.hpp"

class ResponseBuilder {
	private:
		static Response handleGet(const Request &req, const std::string &path);
		static Response handlePost(const Request &req, const std::string &path);
		static Response handleDelete(const Request &req, const std::string &path);
		static Response serveStaticFile(const Request &req, const std::string &filePath);
		static Response buildErrorResponse(int code, const std::string &msg);
		static bool shouldCloseConnection(const Request &req, int statusCode);

		static Response handlePostDirect(const Request &req, const std::string &path);

	public:
		//! Para poderlo usar en el main lo pongo en public
		static Response handleError(const Request &req, int errorCode);
		//* Esta función se va a encargar de montar la respuesta
		static Response buildResponse(const Request &req, const std::string &path);
};