#include "network/Server.hpp"

Server::Server(int port)
	: _socket(new Socket(port))
{ }

Server::Server() : _socket(NULL)
{ }

Server::Server(const Server &other)
	: _socket(other._socket)
	, _clients(other._clients)
	, _pollFds(other._pollFds)
{ }
Server &Server::operator=(const Server &other)
{
	if (this != &other)
	{
		_socket = other._socket;
		_clients = other._clients;
		_pollFds = other._pollFds;
	}
	return *this;
}

Server::~Server()
{
	for (std::vector<Client *>::iterator it = _clients.begin();
		 it != _clients.end(); ++it)
	{
		delete *it;
	}

	delete _socket;
}

void Server::start()
{
	_socket->create();
	_socket->setReuseAddr();
	_socket->setNonBlocking();
	_socket->bindSocket();
	_socket->listenSocket();
	struct pollfd serverPollFd;
	serverPollFd.fd = _socket->getFd();
	serverPollFd.events = POLLIN;
	serverPollFd.revents = 0;
	_pollFds.push_back(serverPollFd);
	std::cout << "Server listening on port 8080" << std::endl;
}

void Server::run()
{
	while (true)
	{
		int result = poll(
			&_pollFds[0],
			_pollFds.size(),
			1000
		);
		if (result == -1)
			throw std::runtime_error("poll() failed");
		if (result == 0)
		{
			checkTimeouts();
			continue;
		}
		for (size_t i = 0; i < _pollFds.size(); ++i)
		{
			if (_pollFds[i].revents == 0)
				continue;
			handlePollEvent(i);
		}
		checkTimeouts();
	}
}

void Server::handlePollEvent(size_t index)
{
	if (_pollFds[index].fd == _socket->getFd())
	{
		handleServerEvent(index);
		return;
	}
	handleClientEvent(index);
}

void Server::handleServerEvent(size_t index)
{
	short revents = _pollFds[index].revents;

	if (revents & (POLLERR | POLLHUP | POLLNVAL))
		throw std::runtime_error(
			"Server socket error"
		);

	if (revents & POLLIN)
		addClient();
}

void Server::handleClientEvent(size_t index)
{
	short revents = _pollFds[index].revents;
	if (revents & (POLLERR | POLLHUP | POLLNVAL))
	{
		std::cout << "Client disconnected: "
				  << _pollFds[index].fd
				  << std::endl;
		removeClient(index);
		return;
	}
	if (revents & POLLIN)
	{
		handleClientRead(index);
		if (index >= _pollFds.size())
			return;
	}
	if (revents & POLLOUT)
		handleClientWrite(index);
}

void Server::handleClientRead(size_t index)
{
	Client *client = findClient(_pollFds[index].fd);
	if (client == NULL)
		return;
	if (!client->receive())
	{
		std::cout << "Client disconnected: "
				  << client->getFd()
				  << std::endl;
		removeClient(index);
		return;
	}
	std::string request;
	if (!client->extractRequest(request))
		return;
	std::cout << "Request received:"
			  << std::endl;
	std::cout << request << std::endl;
	std::string response =
		"HTTP/1.1 200 OK\r\n"
		"Content-Length: 12\r\n"
		"Content-Type: text/plain\r\n"
		"\r\n"
		"Hello World!";
	client->setResponse(response);
	_pollFds[index].events |= POLLOUT;
}

void Server::handleClientWrite(size_t index)
{
	Client *client = findClient(_pollFds[index].fd);
	if (client == NULL)
		return;
	if (!client->sendData())
	{
		std::cout << "Send failed: "
				  << client->getFd()
				  << std::endl;
		removeClient(index);
		return;
	}
	if (!client->hasDataToSend())
		_pollFds[index].events &= ~POLLOUT;
}

Client *Server::findClient(int fd)
{
	for (size_t i = 0; i < _clients.size(); ++i)
	{
		if (_clients[i]->getFd() == fd)
			return _clients[i];
	}

	return NULL;
}

void Server::removeClient(int index)
{
	int fd = _pollFds[index].fd;

	for (std::vector<Client *>::iterator it = _clients.begin();
		 it != _clients.end(); ++it)
	{
		if ((*it)->getFd() == fd)
		{
			delete *it;
			_clients.erase(it);
			break;
		}
	}

	_pollFds.erase(_pollFds.begin() + index);
}

void Server::addClient()
{
	int clientFd = _socket->acceptConnection();

	if (clientFd == -1)
		return;

	Client *client = new Client(clientFd);

	_clients.push_back(client);

	struct pollfd clientPollFd;

	clientPollFd.fd = clientFd;
	clientPollFd.events = POLLIN;
	clientPollFd.revents = 0;

	_pollFds.push_back(clientPollFd);

	std::cout << "Client connected: "
			  << clientFd
			  << std::endl;
}

void Server::checkTimeouts()
{
	std::time_t now = std::time(NULL);

	for (size_t i = 1; i < _pollFds.size(); ++i)
	{
		for (size_t j = 0; j < _clients.size(); ++j)
		{
			if (_clients[j]->getFd() == _pollFds[i].fd)
			{
				if (_clients[j]->isTimedOut(now, CLIENT_TIMEOUT))
				{
					std::cout << "Client timeout: "
							  << _pollFds[i].fd
							  << std::endl;

					removeClient(i);
					--i;
				}

				break;
			}
		}
	}
}