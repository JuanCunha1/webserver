#include "network/Server.hpp"



Server::Server()
    :	_sockets(),
    	_clients(),
		_pollFds()
{
}

Server::Server(const Server &other)
	: _sockets(other._sockets)
	, _clients(other._clients)
	, _pollFds(other._pollFds)
{ }
Server &Server::operator=(const Server &other)
{
	if (this != &other)
	{
		_sockets = other._sockets;
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
	for (std::vector<Socket *>::iterator it = _sockets.begin();
		 it != _sockets.end(); ++it)
	{
		delete *it;
	}
}
void Server::start(const std::vector<int> &ports)
{
	for (size_t i = 0; i < ports.size(); ++i)
	{
		Socket *socket = new Socket(ports[i]);

		try
		{
			socket->create();
			socket->setReuseAddr();
			socket->setNonBlocking();
			socket->bindSocket();
			socket->listenSocket();

			addListeningSocket(socket);
		}
		catch (...)
		{
			delete socket;
			throw;
		}
	}
	std::cout << "Server started with "
			  << _sockets.size()
			  << " listening socket(s)"
			  << std::endl;
}

void Server::addListeningSocket(Socket *socket)
{
	struct pollfd pfd;

	pfd.fd = socket->getFd();
	pfd.events = POLLIN;
	pfd.revents = 0;

	_pollFds.push_back(pfd);
	_sockets.push_back(socket);
}

Socket *Server::findListeningSocket(int fd)
{
	for (size_t i = 0; i < _sockets.size(); ++i)
	{
		if (_sockets[i]->getFd() == fd)
			return _sockets[i];
	}

	return NULL;
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
	int fd = _pollFds[index].fd;

	if (_pollFds[index].revents & POLLIN)
	{
		for (size_t i = 0; i < _sockets.size(); ++i)
		{
			if (_sockets[i]->getFd() == fd)
			{
				handleServerEvent(index);
				return;
			}
		}

		handleClientEvent(index);
	}
}

void Server::handleServerEvent(size_t index)
{
	addClient(index);
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
void Server::addClient(size_t index)
{
	int clientFd = _sockets[index]->acceptConnection();

	if (clientFd == -1)
		return;

	Client *client = new Client(
		clientFd,
		_sockets[index]->getPort()
	);

	_clients.push_back(client);

	struct pollfd clientPollFd;

	clientPollFd.fd = clientFd;
	clientPollFd.events = POLLIN;
	clientPollFd.revents = 0;

	_pollFds.push_back(clientPollFd);
}

void Server::checkTimeouts()
{
	std::time_t now = std::time(NULL);
	size_t pollSize = _pollFds.size();
	size_t clientSize = _clients.size();
	for (size_t i = 1; i < pollSize; ++i)
	{
		for (size_t j = 0; j < clientSize; ++j)
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