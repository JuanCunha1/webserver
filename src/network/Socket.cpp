#include "network/Socket.hpp"
#include <sstream> // A: for std::stringstream

Socket::Socket(int port, const std::string &host)
	: _fd(-1), _port(port), _host(host)
{
	std::cout << "host: " << _host << std::endl;
}

Socket::Socket()
	: _fd(-1), _port(-1), _host("127.0.0.1")
{
}

Socket::Socket(const Socket &other)
	: _fd(other._fd), _port(other._port), _host(other._host)
{
}

Socket &Socket::operator=(const Socket &other)
{
	if (this != &other)
	{
		_fd = other._fd;
		_port = other._port;
		_host = other._host;
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

// A: prohibited functions here
/*void Socket::bindSocket()
{
	struct sockaddr_in address;

	std::memset(&address, 0, sizeof(address));

	address.sin_family = AF_INET;
	if (_host == "localhost")
    address.sin_addr.s_addr = inet_addr("127.0.0.1");
	else if (_host == "0.0.0.0")
		address.sin_addr.s_addr = htonl(INADDR_ANY);
	else
		address.sin_addr.s_addr = inet_addr(_host.c_str());
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
*/

void Socket::bindSocket()
{
	struct addrinfo hints;
	struct addrinfo *res;

	// Manually initialize to 0/NULL to avoid using std::memset (which is forbidden)
	hints.ai_flags = 0;
	hints.ai_family = AF_INET;       // IPv4
	hints.ai_socktype = SOCK_STREAM; // TCP
	hints.ai_protocol = 0;
	hints.ai_addrlen = 0;
	hints.ai_addr = NULL;
	hints.ai_canonname = NULL;
	hints.ai_next = NULL;

	// getaddrinfo requires the port in text format (string)
	std::stringstream ss;
	ss << _port;
	std::string portStr = ss.str();

	// Host handling
	std::string targetHost = _host;
	const char* hostPtr;

	if (targetHost == "0.0.0.0")
	{
		hints.ai_flags = AI_PASSIVE; // Listen on all interfaces
		hostPtr = NULL;
	} 
	else
	{
		if (targetHost == "localhost") {
			targetHost = "127.0.0.1";
		}
		hostPtr = targetHost.c_str();
	}
	// Call getaddrinfo (ALLOWED)
	int status = getaddrinfo(hostPtr, portStr.c_str(), &hints, &res);
	if (status != 0)
	{
		// gai_strerror is allowed by the subject
		std::cerr << "getaddrinfo error: " << gai_strerror(status) << std::endl;
		throw std::runtime_error("getaddrinfo failed");
	}
	if (bind(_fd, res->ai_addr, res->ai_addrlen) == -1)
	{
		freeaddrinfo(res); // VERY IMPORTANT: free memory before throwing
		std::cerr << "Error: bind() failed: " << std::strerror(errno) << std::endl;
		throw std::runtime_error("bind() failed");
	}
	// Free the linked list of addresses (ALLOWED)
	freeaddrinfo(res);
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
		(struct sockaddr *)&clientAddress,
		&clientAddressLength);
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

int Socket::getPort() const
{
	return _port;
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