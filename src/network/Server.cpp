#include "network/Server.hpp"

Server::Server()
	:	_sockets(),
		_clients(),
		_pollFds(),
		_serverConfigs()
{
}

Server::Server(std::vector<ConfigServer> serverConfigs)
	:	_sockets(),
		_clients(),
		_pollFds(),
		_serverConfigs(serverConfigs)
{
}

Server::Server(const Server &other)
	: _sockets(other._sockets)
	, _clients(other._clients)
	, _pollFds(other._pollFds)
	, _serverConfigs(other._serverConfigs)
{ }
Server &Server::operator=(const Server &other)
{
	if (this != &other)
	{
		_sockets = other._sockets;
		_clients = other._clients;
		_pollFds = other._pollFds;
		_serverConfigs = other._serverConfigs;
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
void Server::start()
{
	for (size_t i = 0; i < _serverConfigs.size(); ++i)
	{
		const ConfigServer &config = _serverConfigs[i];
		Socket *socket = new Socket(config.port, config.host);

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
		size_t i = 0;

		while (i < _pollFds.size())
		{
			if (_pollFds[i].revents == 0)
			{
				++i;
				continue;
			}

			size_t oldSize = _pollFds.size();

			handlePollEvent(i);

			if (_pollFds.size() == oldSize)
				++i;
		}
		checkTimeouts();
	}
}

bool Server::isListeningSocket(int fd) const
{
	for (size_t i = 0; i < _sockets.size(); ++i)
	{
		if (_sockets[i]->getFd() == fd)
			return true;
	}

	return false;
}

void Server::handleServerError(size_t index)
{
	int fd = _pollFds[index].fd;

	std::cerr << "Error on listening socket fd "
			  << fd << std::endl;

	for (size_t i = 0; i < _sockets.size(); ++i)
	{
		if (_sockets[i]->getFd() == fd)
		{
			delete _sockets[i];
			_sockets.erase(_sockets.begin() + i);
			break;
		}
	}

	_pollFds.erase(_pollFds.begin() + index);
}

void Server::handlePollEvent(size_t index)
{
	int fd = _pollFds[index].fd;
	short revents = _pollFds[index].revents;

	if (isListeningSocket(fd))
	{
		if (revents & (POLLERR | POLLHUP | POLLNVAL))
		{
			handleServerError(index);
			return;
		}
		if (revents & POLLIN)
			handleServerEvent(index);
		return;
	}
	if (revents & (POLLERR | POLLHUP | POLLNVAL))
	{
		removeClient(index);
		return;
	}
	if (revents & POLLIN)
	{
		handleClientRead(index);
		return;
	}
	if (revents & POLLOUT)
		handleClientWrite(index);

}

void Server::handleServerEvent(size_t index)
{
	int fd = _pollFds[index].fd;

	for (size_t i = 0; i < _sockets.size(); ++i)
	{
		if (_sockets[i]->getFd() == fd)
		{
			addClient(i);
			return;
		}
	}
}

void Server::handleClientEvent(size_t index)
{
	if (index >= _pollFds.size())
		return;

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

std::string createTestResponse()
{
	return "HTTP/1.1 200 OK\r\n"
		   "Content-Length: 13\r\n"
		   "Content-Type: text/plain\r\n"
		   "Connection: keep-alive\r\n"
		   "\r\n"
		   "Hello, World!\r\n";
}

void Server::handleClientRead(size_t index)
{
	Client *client = findClient(_pollFds[index].fd);

	if (client == NULL)
	{
		removeClient(index);
		return;
	}

	int result = client->receive();

	if (result == 0)
	{
		removeClient(index);
		return;
	}
	if (result == -1)
		return;
	if (result == -2)
	{
		removeClient(index);
		return;
	}
	std::string request;

	if (!client->extractRequest(request))
		return;

	// Por enquanto apenas teste
	client->setResponse(createTestResponse());

	_pollFds[index].events |= POLLOUT;
}
/*
void Server::handleClientWrite(size_t index)
{
	Client *client = findClient(_pollFds[index].fd);

	if (client == NULL)
	{
		removeClient(index);
		return;
	}
	int result = client->sendResponse();

	if (result < 0)
	{
		removeClient(index);
		return;
	}

	if (client->responseComplete())
	{
		removeClient(index);
		return;
	}

	_pollFds[index].events |= POLLOUT;
}
*/
void Server::handleClientWrite(size_t index)
{
	Client *client = findClient(_pollFds[index].fd);
	if (client == NULL)
	{
		removeClient(index);
		return;
	}
	
	int result = client->sendData();
	if (result < 0)
	{
		removeClient(index);
		return;
	}
	_pollFds[index].events = POLLIN;
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

void Server::removeClient(size_t index)
{
	if (index >= _pollFds.size())
		return;

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

void Server::addClient(size_t socketIndex)
{
	int clientFd = _sockets[socketIndex]->acceptConnection();

	if (clientFd == -1)
		return;

	Client *client = new Client(
		clientFd,
		_sockets[socketIndex]->getPort()
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

	size_t i = 0;

	while (i < _clients.size())
	{
		if (!_clients[i]->isTimedOut(now, CLIENT_TIMEOUT))
		{
			++i;
			continue;
		}

		int fd = _clients[i]->getFd();

		std::cout << "Client timeout: "
				  << fd
				  << std::endl;

		for (size_t j = 0; j < _pollFds.size(); ++j)
		{
			if (_pollFds[j].fd == fd)
			{
				removeClient(j);
				break;
			}
		}
	}
}