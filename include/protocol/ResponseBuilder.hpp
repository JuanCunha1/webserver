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

		// buildResponse helpers
		Response		handleRedirection();
		bool			isMethodAllowed(const std::string &method) const;
		bool			resolvePath();
		std::string		stripLocationPrefix(const std::string &uri) const;
		HandlerResult	dispatchMethod(const std::string &method);

		HandlerResult handleGet();
		HandlerResult handlePost();
		HandlerResult handleDelete();
		Response serveStaticFile(const std::string &filePath);

		// GET helpers
		Response		statErrorToResponse(int err);
		HandlerResult	handleDirectory();
		std::string		resolveIndexPath() const;
		Response		serveAutoindex();
		Response		buildHtmlResponse(const std::string &html);
		HandlerResult	handleRegularFile();
		HandlerResult	launchCgi(const std::string &cgiBinary);
		
		bool shouldCloseConnection(int statusCode);
		
		// POST helpers
		HandlerResult	handleMultipart(const std::string &contentType);
		std::string		resolveUploadDir() const;
		HandlerResult	handleUrlEncoded();
		Response		handlePostDirect();
		std::string		resolveDirectTargetPath() const;
		Response		buildPostResponse(int statusCode, const std::string &statusMsg,
										const std::string &html);

		bool	isCgiRequest();
		bool	isCgiExtension(const std::string& ext, std::string& outCgiBinary);
		
		std::string normalizeUri(const std::string& uri);

		std::string getCustomErrorPage(int errorCode);
		static std::string getDefaultErrorPage(int errorCode, const std::string& statusMsg);

	public:
		ResponseBuilder(const Request &request, const ConfigLocation &location, const ConfigServer &server);
		~ResponseBuilder();

		Response handleError(int errorCode);

		static std::string getDefaultStatusMessage(int statusCode);

		static Response buildErrorResponse(int code, const std::string &msg);

		HandlerResult buildResponse();
};