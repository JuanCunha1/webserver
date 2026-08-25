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
		Socket						*_socket;
		std::vector<Client *>		_clients;
		std::vector<struct pollfd>	_pollFds;
		Server();
		Server(const Server &other);
		Server &operator=(const Server &other);

		void addClient();
		void removeClient(int index);
	public:
		Server(int port);
		~Server();

		void checkTimeouts();

		void start();
		void run();
};