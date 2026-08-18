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

class Server
{
	private:
		Socket *_socket;
		std::vector<Client *> _clients;

		Server(const Server &other);
		Server &operator=(const Server &other);

	public:
		Server(int port);
		~Server();

		void start();
		void acceptClient();
};