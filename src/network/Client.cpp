#include "network/Client.hpp"

Client::Client(int fd)
	: _fd(fd), _lastActivity(std::time(NULL))
	, _requestBuffer(""), _responseBuffer("")
{
}

Client::Client()
	: _fd(-1), _lastActivity(std::time(NULL))
	, _requestBuffer(""), _responseBuffer("")
{
}

Client::Client(const Client &other)
	: _fd(other._fd)
	, _lastActivity(other._lastActivity)
	, _requestBuffer(other._requestBuffer)
	, _responseBuffer(other._responseBuffer)
	
{
}

Client &Client::operator=(const Client &other)
{
	if (this != &other)
	{
		_fd = other._fd;
		_lastActivity = other._lastActivity;
		_requestBuffer = other._requestBuffer;
		_responseBuffer = other._responseBuffer;
		
	}
	return *this;
}

Client::~Client()
{
	if (_fd != -1)
		close(_fd);
}

int Client::getFd() const
{
	return _fd;
}

bool Client::receive()
{
	char buffer[4096];

	int bytesRead = recv(
		_fd,
		buffer,
		sizeof(buffer),
		0
	);

	if (bytesRead > 0)
	{
		_requestBuffer.append(buffer, bytesRead);
		_lastActivity = std::time(NULL);
		return true;
	}

	if (bytesRead == 0)
		return false;

	if (errno == EAGAIN || errno == EWOULDBLOCK)
		return true;

	return false;
}

bool Client::extractRequest(std::string &request)
{
	std::string::size_type end =
		_requestBuffer.find("\r\n\r\n");

	if (end == std::string::npos)
		return false;

	end += 4;

	request = _requestBuffer.substr(0, end);

	_requestBuffer.erase(0, end);

	return true;
}

void Client::setResponse(const std::string &response)
{
	_responseBuffer = response;
}

bool Client::hasDataToSend() const
{
	return !_responseBuffer.empty();
}

bool Client::sendData()
{
	if (_responseBuffer.empty())
		return true;

	int bytesSent = send(
		_fd,
		_responseBuffer.c_str(),
		_responseBuffer.size(),
		0);
	if (bytesSent > 0)
	{
		_responseBuffer.erase(0, bytesSent);
		_lastActivity = std::time(NULL);
		return true;
	}
	if (bytesSent == -1)
	{
		if (errno == EAGAIN || errno == EWOULDBLOCK)
			return true;
		return false;
	}
	return false;
}

const std::string &Client::getRequest() const
{
	return _requestBuffer;
}

bool Client::hasDataToReceive() const
{
	return !_requestBuffer.empty();
}

bool Client::isTimedOut(std::time_t now, int timeout) const
{
	return (now - _lastActivity) >= timeout;
}