#pragma once

#include <unistd.h>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <cerrno>
#include <ctime>

class Client
{
	private:
		int			_fd;
		std::string	_requestBuffer;
		std::string	_responseBuffer;
		std::time_t	_lastActivity;
		
		Client(const Client &other);
		Client &operator=(const Client &other);

	public:
		Client(int fd);
		~Client();

		int getFd() const;

		bool receive();
		bool sendData();

		void setResponse(const std::string &response);

		bool hasDataToSend() const;
		bool hasDataToReceive() const;

		const std::string &getRequest() const;

		bool isTimedOut(std::time_t now, int timeout) const;

		bool extractRequest(std::string &request);
};

