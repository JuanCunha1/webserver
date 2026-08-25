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
			short revents = _pollFds[i].revents;

			if (revents == 0)
				continue;

			/*
			 * Listening socket
			 */
			if (_pollFds[i].fd == _socket->getFd())
			{
				if (revents & (POLLERR | POLLHUP | POLLNVAL))
					throw std::runtime_error(
						"Server socket error"
					);

				if (revents & POLLIN)
					addClient();

				continue;
			}

			/*
			 * Client error / disconnect
			 */
			if (revents & (POLLERR | POLLHUP | POLLNVAL))
			{
				std::cout << "Client disconnected: "
						  << _pollFds[i].fd
						  << std::endl;

				removeClient(i);

				if (i > 0)
					--i;

				continue;
			}

			/*
			 * Find Client
			 */
			Client *client = NULL;

			for (size_t j = 0; j < _clients.size(); ++j)
			{
				if (_clients[j]->getFd() == _pollFds[i].fd)
				{
					client = _clients[j];
					break;
				}
			}

			if (client == NULL)
				continue;

			/*
			 * READ
			 */
			if (revents & POLLIN)
			{
				if (!client->receive())
				{
					std::cout << "Client disconnected: "
							  << client->getFd()
							  << std::endl;

					removeClient(i);

					if (i > 0)
						--i;

					continue;
				}

				std::string request;

				if (client->extractRequest(request))
				{
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

					_pollFds[i].events |= POLLOUT;
				}
			}

			/*
			 * WRITE
			 */
			if (revents & POLLOUT)
			{
				if (!client->sendData())
				{
					std::cout << "Send failed: "
							  << client->getFd()
							  << std::endl;

					removeClient(i);

					if (i > 0)
						--i;

					continue;
				}

				if (!client->hasDataToSend())
					_pollFds[i].events &= ~POLLOUT;
			}
		}

		checkTimeouts();
	}
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