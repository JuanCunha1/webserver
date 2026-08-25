#include "network/Socket.hpp"


Socket::Socket(int port)
	: _fd(-1), _port(port)
{
}

Socket::Socket()
	: _fd(-1), _port(-1)
{
}

Socket::Socket(const Socket &other)
	: _fd(other._fd), _port(other._port)
{
}

Socket &Socket::operator=(const Socket &other)
{
	if (this != &other)
	{
		_fd = other._fd;
		_port = other._port;
	}
	return *this;
}

Socket::~Socket()
{
	if (_fd != -1)
		close(_fd);
}

void Socket::create()
{
	_fd = socket(AF_INET, SOCK_STREAM, 0);

	if (_fd == -1)
	{
		std::cerr << "Error: socket() failed: "
				  << std::strerror(errno) << std::endl;
		throw std::runtime_error("socket() failed");
	}
}

void Socket::bindSocket()
{
	struct sockaddr_in address;

	std::memset(&address, 0, sizeof(address));

	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(_port);

	if (bind(_fd,
			reinterpret_cast<struct sockaddr *>(&address),
			sizeof(address)) == -1)
	{
		std::cerr << "Error: bind() failed: "
				  << std::strerror(errno) << std::endl;
		throw std::runtime_error("bind() failed");
	}
}

void Socket::listenSocket()
{
	if (listen(_fd, 10) == -1)
	{
		std::cerr << "Error: listen() failed: "
				  << std::strerror(errno) << std::endl;
		throw std::runtime_error("listen() failed");
	}
}

int Socket::acceptConnection()
{
	struct sockaddr_in clientAddress;
	socklen_t clientAddressLength = sizeof(clientAddress);

	int clientFd = accept(
		_fd,
		reinterpret_cast<struct sockaddr *>(&clientAddress),
		&clientAddressLength
	);

	if (clientFd == -1)
	{
		if (errno == EAGAIN || errno == EWOULDBLOCK)
			return -1;

		throw std::runtime_error("accept() failed");
	}

	return clientFd;
}

int Socket::getFd() const
{
	return _fd;
}

void Socket::setNonBlocking()
{
	int flags = fcntl(_fd, F_GETFL, 0);

	if (flags == -1)
		throw std::runtime_error("fcntl(F_GETFL) failed");

	if (fcntl(_fd, F_SETFL, flags | O_NONBLOCK) == -1)
		throw std::runtime_error("fcntl(F_SETFL) failed");
}

void Socket::setReuseAddr()
{
	int option = 1;

	if (setsockopt(
			_fd,
			SOL_SOCKET,
			SO_REUSEADDR,
			&option,
			sizeof(option)) == -1)
	{
		throw std::runtime_error("setsockopt() failed");
	}
}