#pragma once

#include "protocol/RequestParser.hpp"
#include <unistd.h>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <cerrno>
#include <ctime>
#include <iostream>

class Client
{
	private:
		int				_fd;
		int				_serverPort;
		std::time_t		_lastActivity;
		RequestParser	_parser;
		std::string		_responseBuffer;
		
		
		Client();
		Client(const Client &other);
		Client &operator=(const Client &other);

	public:
		Client(int fd, int serverPort, size_t maxBodySize);
		~Client();

		int getFd() const;
		int getServerPort() const;

		int receive();
		int sendData();

		void setResponse(const std::string &response);

		bool hasDataToSend() const;

		const std::string &getRequest() const;

		bool isTimedOut(std::time_t now, int timeout) const;


		RequestParser &getParser();
		const RequestParser &getParser() const;
};

