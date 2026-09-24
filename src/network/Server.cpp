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
		if (index >= _pollFds.size())
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

	try
	{
		client->getParser().process();
	}
	catch (const std::exception &e)
	{
		std::cerr << "HTTP parse error: "
				  << e.what()
				  << std::endl;
		return;
	}

	if (client->getParser().getState() != RequestParser::COMPLETE)
		return;

	const Request &request = client->getParser().getRequest();
	
	ConfigServer *serverConfig =
		findServerConfig(client->getServerPort());

	if (serverConfig == NULL)
	{
		std::cerr << "No server config for port "
				  << client->getServerPort()
				  << std::endl;
		return;
	}

	ConfigLocation *location =
		findLocation(*serverConfig, request.getUri());

	if (location == NULL)
	{
		std::cerr << "No location for URI: "
				  << request.getUri()
				  << std::endl;
		return;
	}

	ResponseBuilder builder(request, *location);

	HandlerResult handlerResult = builder.buildResponse();

	if (handlerResult.isCgi)
	{
		
		return;
	}

	std::string response;

	response += handlerResult.staticResponse.getHeadersAsString();
	response += handlerResult.staticResponse.getBody();

	client->setResponse(response);

	_pollFds[index].events |= POLLOUT;
}

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
		_sockets[socketIndex]->getPort(),
		_serverConfigs[socketIndex].clientMaxBodySize
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

ConfigServer *Server::findServerConfig(int port)
{
	for (size_t i = 0; i < _serverConfigs.size(); ++i)
	{
		if (_serverConfigs[i].port == port)
			return &_serverConfigs[i];
	}

	return NULL;
}

ConfigLocation *Server::findLocation(ConfigServer &server,
									 const std::string &uri)
{
	ConfigLocation *bestMatch = NULL;
	size_t bestLength = 0;

	for (size_t i = 0; i < server.locations.size(); ++i)
	{
		const ConfigLocation &location = server.locations[i];

		if (location.path.empty())
			continue;

		bool match = false;

		if (location.path == "/")
		{
			match = true;
		}
		else if (uri == location.path)
		{
			match = true;
		}
		else if (uri.compare(0, location.path.length(),
								location.path) == 0)
		{
			if (location.path[location.path.length() - 1] == '/' ||
				(uri.length() > location.path.length() && uri[location.path.length()] == '/')
			)
			{
				match = true;
			}
		}

		if (match && location.path.length() > bestLength)
		{
			bestMatch = &server.locations[i];
			bestLength = location.path.length();
		}
	}

	return bestMatch;
}