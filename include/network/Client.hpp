#pragma once

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
		int			_fd;
		int			_serverPort;
		std::time_t	_lastActivity;
		std::string	_requestBuffer;
		std::string	_responseBuffer;
		
		
		Client();
		Client(const Client &other);
		Client &operator=(const Client &other);

	public:
		Client(int fd, int serverPort);
		~Client();

		int getFd() const;
		int getServerPort() const;

		bool receive();
		bool sendData();

		void setResponse(const std::string &response);

		bool hasDataToSend() const;
		bool hasDataToReceive() const;

		const std::string &getRequest() const;

		bool isTimedOut(std::time_t now, int timeout) const;

		bool extractRequest(std::string &request);
};

