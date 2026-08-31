#pragma once

#include "Request.hpp"
#include "Response.hpp"

class ResponseBuilder {
	private:

		//!Igual deberia añadir un puntero a response o algo así??

		Response handleGet(const Request &req, const std::string &path);
		Response handlePost(const Request &req, const std::string &path);
		Response handleDelete(const Request &req, const std::string &path);
		Response handleError(const Request &req, int errorCode);

	public:
		//! Esta función se va a encargar de montar la respuesta
		Response buildResponse(const Request &req, const std::string &path);
};