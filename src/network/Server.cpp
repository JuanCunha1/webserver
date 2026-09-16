#include "network/Server.hpp"



Server::Server()
	:	_sockets(),
		_clients(),
		_pollFds(),
		_request(),
		_state(RequestParser::REQUEST_LINE)
{
}

Server::Server(const Server &other)
	: _sockets(other._sockets)
	, _clients(other._clients)
	, _pollFds(other._pollFds)
	, _request(other._request)
	, _state(other._state)
{ }
Server &Server::operator=(const Server &other)
{
	if (this != &other)
	{
		_sockets = other._sockets;
		_clients = other._clients;
		_pollFds = other._pollFds;
		_request = other._request;
		_state = other._state;
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
		Socket *socket = new Socket(ports[i], "127.0.0.1");

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
			  << ports[0]
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

void Server::handlePollEvent(size_t index)
{
	int fd = _pollFds[index].fd;
	short revents = _pollFds[index].revents;

	if (revents & (POLLERR | POLLHUP | POLLNVAL))
	{
		removeClient(index);
		return;
	}

	if (revents & POLLIN)
	{
		for (size_t i = 0; i < _sockets.size(); ++i)
		{
			if (_sockets[i]->getFd() == fd)
			{
				handleServerEvent(index);
				return;
			}
		}

		handleClientRead(index);

		if (index >= _pollFds.size())
			return;
	}

	if (revents & POLLOUT)
	{
		handleClientWrite(index);
	}
}

void Server::handleServerEvent(size_t index)
{
	addClient(index);
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
		   "Content-Length: 10\r\n"
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
	std::cout << "Received data from client fd: "
			  << client->getFd()
			  << std::endl;
	if (result == 0)
	{
		removeClient(index);
		return;
	}

	if (result < 0)
		return;

	std::string request;

	if (!client->extractRequest(request))
	{
		return;
	}
	// Passing buffer to request parser
	//process(request, &_request, &_state, clientMaxbodySize);



	//std::cout << request << std::endl;
	// test response for now, you can replace this with your actual response generation logic
	
	client->setResponse(createTestResponse());

	_pollFds[index].events |= POLLOUT;
}
/*void Server::handleClientWrite(size_t index)
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
}*/
void Server::handleClientWrite(size_t index)
{
	Client *client = findClient(_pollFds[index].fd);
	if (client == NULL)
	{
		removeClient(index);
		return;
	}
	
	client->sendData();
	
	if (!client->hasDataToSend())
	{
		removeClient(index);
		return;
	}
	_pollFds[index].events = POLLIN | POLLOUT;
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