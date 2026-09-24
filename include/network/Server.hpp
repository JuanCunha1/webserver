#pragma once

#include <vector>

#include "network/Socket.hpp"
#include "network/Client.hpp"

#include <iostream>
#include <stdexcept>
#include <vector>
#include <poll.h>
#include "protocol/Request.hpp"
#include "protocol/RequestParser.hpp"
#include "config/ConfigParser.hpp"

class Client;
class Socket;

static const int CLIENT_TIMEOUT = 60;

class Server
{
	private:
		std::vector<Socket *>		_sockets;
		std::vector<Client *>		_clients;
		std::vector<struct pollfd>	_pollFds;
        std::vector<ConfigServer>   _serverConfigs;
		
		Server();
		Server(const Server &other);
		Server &operator=(const Server &other);

		void	addClient(size_t socketIndex);
		void	removeClient(size_t index);

		void	handlePollEvent(size_t index);
		void	handleServerEvent(size_t index);
		void	handleClientEvent(size_t index);
		void	handleClientRead(size_t index);
		void	handleClientWrite(size_t index);
        void    handleServerError(size_t index);
    
		void	addListeningSocket(Socket *socket);
        bool    isListeningSocket(int fd) const;
		Socket	*findListeningSocket(int fd);
		Client	*findClient(int fd);
	public:
		Server(std::vector<ConfigServer> serverConfigs);
		~Server();

		void checkTimeouts();

		void start();
		void run();
};