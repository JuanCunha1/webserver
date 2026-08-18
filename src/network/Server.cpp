#include "network/Server.hpp"
Server::Server(int port)
	: _socket(new Socket(port))
{
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
	_socket->setNonBlocking();
	_socket->bindSocket();
	_socket->listenSocket();

	std::cout << "Server listening on port 8080" << std::endl;
}

void Server::acceptClient()
{
	int clientFd = _socket->acceptConnection();

	if (clientFd == -1)
		return;

	Client *client = new Client(clientFd);
	_clients.push_back(client);

	std::cout << "Client connected: "
			  << client->getFd() << std::endl;
}