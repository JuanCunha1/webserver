#pragma once

#include <vector>

#include "network/Socket.hpp"
#include "network/Client.hpp"

#include <iostream>
#include <stdexcept>
#include <vector>
#include <poll.h>

class Client;
class Socket;

static const int CLIENT_TIMEOUT = 60;

class Server
{
	private:
		std::vector<Socket *>		_sockets;
		std::vector<Client *>		_clients;
		std::vector<struct pollfd>	_pollFds;
		
		
		Server(const Server &other);
		Server &operator=(const Server &other);

		void	addClient(size_t index);
		void	removeClient(int index);

		void	handlePollEvent(size_t index);
		void	handleServerEvent(size_t index);
		void	handleClientEvent(size_t index);
		void	handleClientRead(size_t index);
		void	handleClientWrite(size_t index);

		void	addListeningSocket(Socket *socket);
		Socket	*findListeningSocket(int fd);
		Client	*findClient(int fd);
	public:
		Server();
		~Server();

		void checkTimeouts();

		void start(const std::vector<int> &ports);
		void run();
};