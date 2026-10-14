#include "network/Client.hpp"

Client::Client(int fd, int serverPort, size_t maxBodySize)
	: _fd(fd), _serverPort(serverPort), _lastActivity(std::time(NULL))
	, _parser(maxBodySize), _responseBuffer(""), _state(READING)
	, _cgiHandler(NULL)
{
}

Client::Client()
	: _fd(-1), _serverPort(-1), _lastActivity(std::time(NULL))
	, _parser(1024), _responseBuffer(""), _state(READING)
	, _cgiHandler(NULL)
{
}

Client::Client(const Client &other)
	: _fd(other._fd)
	, _lastActivity(other._lastActivity)
	, _parser(other._parser)
	, _responseBuffer(other._responseBuffer)
	
{
}

Client &Client::operator=(const Client &other)
{
	if (this != &other)
	{
		_fd = other._fd;
		_lastActivity = other._lastActivity;
		_parser = other._parser;
		_responseBuffer = other._responseBuffer;
		
	}
	return *this;
}

Client::~Client()
{
	if (_fd != -1)
		close(_fd);
	delete _cgiHandler;
	_cgiHandler = NULL;
}

int Client::getServerPort() const
{
	return _serverPort;
}

int Client::getFd() const
{
	return _fd;
}

int Client::receive()
{
	char buffer[4096];

	int bytesRead = recv(_fd, buffer, sizeof(buffer), 0);

	if (bytesRead <= 0)
		return 0;   // connection closed or error: the server will drop the client

	_parser.append(std::string(buffer, bytesRead));
	_lastActivity = std::time(NULL);
	return bytesRead;
}

void Client::setResponse(const std::string &response)
{
	_responseBuffer = response;
}

bool Client::hasDataToSend() const
{
	return !_responseBuffer.empty();
}

int Client::sendData()
{
	if (_responseBuffer.empty())
		return 0;

	ssize_t bytesSent = send(
		_fd,
		_responseBuffer.data(), // data() to handle binary data correctly
		_responseBuffer.size(),
		0
	);

	if (bytesSent <= 0)
		return -1;   // error: the server will drop the client

	_responseBuffer.erase(0, bytesSent);
	_lastActivity = std::time(NULL);
	return static_cast<int>(bytesSent);
}

RequestParser &Client::getParser()
{
	return _parser;
}

const RequestParser &Client::getParser() const
{
	return _parser;
}

bool Client::isTimedOut(std::time_t now, int timeout) const
{
	return (now - _lastActivity) >= timeout;
}