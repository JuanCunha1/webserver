#pragma once

#include <string>
#include <iostream>
#include <cstring>
#include <cerrno>

#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdexcept>
#include <fcntl.h>
#include <cerrno>

class Socket {
	private:
		int _fd;
		int	_port;
		std::string _host;

		Socket(void);
		Socket(const Socket &other);
		Socket &operator=(const Socket &other);

	public:
		Socket(int port, const std::string &host);
		~Socket();

		void	create();
		void	bindSocket();
		void	listenSocket();
		int		acceptConnection();

		int		getPort() const;
		int		getFd() const;
		void	setNonBlocking();
		void	setReuseAddr();
};