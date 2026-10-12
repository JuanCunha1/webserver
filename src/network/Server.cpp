#include "network/Server.hpp"
#include <csignal>

extern volatile sig_atomic_t g_running;

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
		// A: For avoid to start server with no configuration, which will cause a crash when trying to access _serverConfigs[i]
		if (_serverConfigs.empty())
		{
			throw std::runtime_error("Error: No server configurations loaded. Check your .conf file.");
		}
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
	// A: For avoid to start server with no listening sockets, which will cause a crash when trying to access _sockets[i]
	if (_sockets.empty())
	{
		throw std::runtime_error("Error: Could not bind any listening sockets.");
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
	while (g_running)
    {
		int result = poll(
			&_pollFds[0],
			_pollFds.size(),
			1000
		);
		if (result == -1)
		{
			if (errno == EINTR)
			{
				// Poll was interrupted by a signal (like SIGINT), continue the loop to check g_running
				continue;
			}
			throw std::runtime_error("poll() failed");
		}
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
			/*
			// --- ADD THIS HERE ---
        	for (size_t k = 0; k < _pollFds.size(); ++k) {
           		if (_pollFds[k].revents != 0) {
                	std::cout << "[POLL] FD " << _pollFds[k].fd << " despertó con revents: " << _pollFds[k].revents << std::endl;
            	}
       		}
       		// -----------------------
			*/

			size_t oldSize = _pollFds.size();

			handlePollEvent(i);

			/*
			if (_pollFds.size() < oldSize)
				i++;
			*/
			if (_pollFds.size() < oldSize) {
                // 'i' already points to the next correct element.
            } 
            // If the size is equal or GREATER (CGI pipes were added), we do advance.
            else {
                ++i;
            }
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

	/*
    std::cout << "[ROUTER] Evaluando FD " << fd << " | ¿Es CGI?: " << (isCgiFd(fd) ? "SI" : "NO") << std::endl;
    */

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
	if (isCgiFd(fd))
	{
		handleCgiEvent(index);
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

/*
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

	// A: COMMENTED OUT TO ALLOW PIPELINING (BUG 3)
	// if (result == -1)
	// 	return;

	if (result == -2)
	{
		removeClient(index);
		return;
	}

	// I updated this part because it was insufficient when there was a parse error
	// No response was being built
	try
	{
		client->getParser().process();
	}
	catch (const HttpException &e)
	{
		std::cerr << "HTTP parse error: " << e.what() << " (Code: " << e.getStatusCode() << ")" << std::endl;

		Response errorRes = ResponseBuilder::buildErrorResponse(e.getStatusCode(), e.what());
		std::string responseString = errorRes.getHeadersAsString() + errorRes.getBody();

		client->setResponse(responseString);

		// Wake up poll() so it writes the response to the client
		_pollFds[index].events |= POLLOUT;
		return;
	}
	// for cases where it will not be a parse error (may be removable)
	catch (const std::exception &e)
	{
		std::cerr << "Unknown parse error: " << e.what() << std::endl;
		removeClient(index);
		return;
	}

	if (client->getParser().getState() != RequestParser::COMPLETE) {
		// A: Debug message
		// std::cout << "DEBUG IS NON BLOCKING" << std::endl;
		return;
	}

	const Request &request = client->getParser().getRequest();
	// Ainhoa's code
	// 1. Get the value of the "Host" header to pass it to findBestServer
    // (Adjust 'getHeader' to the method name you use in your Request class)
	const std::string *hostValue = request.getHeader("Host");
	std::string hostHeader = hostValue != NULL ? *hostValue : "";

    // Optional: Sometimes the Host header comes with the port (ej: "localhost:8080"). 
    // If your config only stores "localhost", make sure to strip the ":port" from hostHeader.

    // 2. Find the server (Replaces findServerConfig)
    // NOTE: I assume Server holds the vector with all the servers (ej: _servers o _serverConfigs)
    const ConfigServer *serverConfig = 
        UrlMatcher::findBestServer(_serverConfigs, client->getServerPort(), hostHeader);

    if (serverConfig == NULL)
    {
        std::cerr << "No server config for port "
                  << client->getServerPort()
                  << std::endl;
        return; // Ideally a 400 Bad Request error should be returned here
    }

    // 3. Find the Location (Replaces findLocation)
    const ConfigLocation *location = 
        UrlMatcher::findBestLocation(*serverConfig, request.getUri());

    if (location == NULL)
    {
        std::cerr << "No location for URI: "
                  << request.getUri()
                  << std::endl;
        // If you want it to return 404 when there is no location, 
        // you would have to instantiate the ResponseBuilder differently or send the response directly.
        return; 
    }

	ResponseBuilder builder(request, *location, *serverConfig);

	HandlerResult handlerResult = builder.buildResponse();

	if (handlerResult.isCgi)
	{
		// Store the handler in the client
		client->setCgiHandler(handlerResult.cgiHandler);
		client->setState(Client::WAITING_FOR_CGI);

		// Get the read and write FDs of the CgiHandler
		int cgiReadFd = handlerResult.cgiHandler->getReadFd();
		int cgiWriteFd = handlerResult.cgiHandler->getWriteFd();

		// Add the read pipe to poll (POLLIN)
		if (cgiReadFd != -1) {
			struct pollfd pfd;
			pfd.fd = cgiReadFd;
			pfd.events = POLLIN;
			pfd.revents = 0;
			_pollFds.push_back(pfd);
		}

		// If it is a POST and there is a body, add the write pipe to poll (POLLOUT)
		if (cgiWriteFd != -1 && !request.getBody().empty()) {
			struct pollfd pfd;
			pfd.fd = cgiWriteFd;
			pfd.events = POLLOUT;
			pfd.revents = 0;
			_pollFds.push_back(pfd);
		}
		return;
	}

	std::string response;

	response += handlerResult.staticResponse.getHeadersAsString();
	response += handlerResult.staticResponse.getBody();

	client->setResponse(response);

	_pollFds[index].events |= POLLOUT;
}
*/
void Server::handleClientRead(size_t index)
{
	Client *client = findClient(_pollFds[index].fd);
	if (client == NULL)
	{
		removeClient(index);
		return;
	}

	int result = client->receive();
	if (result == 0 || result == -2)
	{
		removeClient(index);
		return;
	}
	// result == -1 (EAGAIN): no new bytes, but we process whatever there is
	processRequest(index);
}

void Server::processRequest(size_t index)
{
	Client *client = findClient(_pollFds[index].fd);
	if (client == NULL)
		return;

	try
	{
		client->getParser().process();
	}
	catch (const HttpException &e)
	{
		std::cerr << "HTTP parse error: " << e.what()
				  << " (Code: " << e.getStatusCode() << ")" << std::endl;
		Response errorRes = ResponseBuilder::buildErrorResponse(e.getStatusCode(), e.what());
		client->setResponse(errorRes.getHeadersAsString() + errorRes.getBody());
		_pollFds[index].events = POLLOUT;   // write only
		return;
	}
	catch (const std::exception &e)
	{
		std::cerr << "Unknown parse error: " << e.what() << std::endl;
		removeClient(index);
		return;
	}

	if (client->getParser().getState() != RequestParser::COMPLETE)
		return;   // bytes missing, keep going with POLLIN

	const Request &request = client->getParser().getRequest();
	const std::string *hostValue = request.getHeader("Host");
	std::string hostHeader = hostValue != NULL ? *hostValue : "";

	const ConfigServer *serverConfig =
		UrlMatcher::findBestServer(_serverConfigs, client->getServerPort(), hostHeader);
	const ConfigLocation *location = (serverConfig != NULL)
		? UrlMatcher::findBestLocation(*serverConfig, request.getUri())
		: NULL;

	if (serverConfig == NULL || location == NULL)
	{
		int code = (serverConfig == NULL) ? 400 : 404;
		Response errorRes = ResponseBuilder::buildErrorResponse(code, code == 400 ? "Bad Request" : "Not Found");
		client->setResponse(errorRes.getHeadersAsString() + errorRes.getBody());
		_pollFds[index].events = POLLOUT;
		return;
	}

	ResponseBuilder builder(request, *location, *serverConfig);
	HandlerResult handlerResult = builder.buildResponse();

	if (handlerResult.isCgi)
	{
		client->setCgiHandler(handlerResult.cgiHandler);
		client->setState(Client::WAITING_FOR_CGI);

		int cgiReadFd = handlerResult.cgiHandler->getReadFd();
		int cgiWriteFd = handlerResult.cgiHandler->getWriteFd();

		// While the CGI works, we do not read from the client
		_pollFds[index].events = 0;

		if (cgiReadFd != -1)
		{
			struct pollfd pfd;
			pfd.fd = cgiReadFd;
			pfd.events = POLLIN;
			pfd.revents = 0;
			_pollFds.push_back(pfd);
		}
		if (cgiWriteFd != -1 && !request.getBody().empty())
		{
			struct pollfd pfd;
			pfd.fd = cgiWriteFd;
			pfd.events = POLLOUT;
			pfd.revents = 0;
			_pollFds.push_back(pfd);
		}
		return;
	}

	client->setResponse(handlerResult.staticResponse.getHeadersAsString()
					  + handlerResult.staticResponse.getBody());
	_pollFds[index].events = POLLOUT;
}

void Server::handleClientWrite(size_t index)
{
	Client *client = findClient(_pollFds[index].fd);
	if (client == NULL)
	{
		removeClient(index);
		return;
	}
	/*
	std::cout << "[DEBUG HTTP] Intentando enviar " << client->getResponseBuffer().size() 
          << " bytes al cliente FD " << client->getFd() << std::endl;
	*/
	// a: Look at the buffer BEFORE sending it to know whether it contains a 4xx/5xx error
	// or whether your teammate's ResponseBuilder explicitly set Connection: close
	bool isErrorOrClose = false;
	std::string respBuffer = client->getResponseBuffer();
	if (respBuffer.find("HTTP/1.1 4") == 0 || 
		respBuffer.find("HTTP/1.1 5") == 0 ||
		respBuffer.find("Connection: close") != std::string::npos)
	{
		isErrorOrClose = true;
	}
	int result = client->sendData();
	if (result < 0)
	{
		removeClient(index);
		return;
	}
	// A: Check has no data to send, if so, reset events to POLLIN and reset parser for next request
	if (!client->hasDataToSend())
	{
		// A: (pipelining) If the client has no more data to send, check whether the request was complete and there were no errors. If everything is fine, reset the parser to allow new requests (pipelining).
		if (client->getParser().getState() != RequestParser::COMPLETE)
		{
			removeClient(index);
			return;
		}
		// A: If we sent a 400/500 error or a forced close, cut the connection outright.
		if (isErrorOrClose)
		{
			removeClient(index);
			return;
		}
		const Request &request = client->getParser().getRequest();
		const std::string *connHeader = request.getHeader("connection");

		if ((connHeader != NULL && *connHeader == "close") || (request.getVersion() == "HTTP/1.0" && (connHeader == NULL || *connHeader != "keep-alive")))
		{
			removeClient(index);
			return;
		}

		_pollFds[index].events = POLLIN;
		client->setState(Client::READING);
		client->getParser().reset();

		processRequest(index);
	}
	else
	{
		_pollFds[index].events |= POLLOUT; // A: |= OR for bits
	}
	/*
	// We only listen again if we have already emptied the whole output buffer
    if (client->hasDataToSend()) {
        _pollFds[index].events |= POLLOUT; 
    } else {
        _pollFds[index].events = POLLIN; 
    }
	*/
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

/*
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
*/

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
		Client::State state = _clients[i]->getState();
		
		// 1. Translate the state for logging
		std::string stateStr;
		if (state == Client::READING) stateStr = "WAITING_FOR_REQUEST";
		else if (state == Client::WAITING_FOR_CGI) stateStr = "WAITING_FOR_CGI";
		else stateStr = "READY_TO_SEND";

		// 2. Structured and improved log
		std::cout << "[WARN] Client timeout after " << CLIENT_TIMEOUT 
				  << "s of inactivity. FD: " << fd 
				  << ", Server Port: " << _clients[i]->getServerPort()
				  << ", State: " << stateStr << std::endl;

		// 3. Courtesy response 408 Request Timeout
		Response errorRes = ResponseBuilder::buildErrorResponse(408, "Request Timeout");
		std::string responseString = errorRes.getHeadersAsString() + errorRes.getBody();
		
		_clients[i]->setResponse(responseString);

		// 4. Change the event to POLLOUT instead of removing the client.
		// The handleClientWrite loop will send the 408 and close the connection gracefully.
		for (size_t j = 0; j < _pollFds.size(); ++j)
		{
			if (_pollFds[j].fd == fd)
			{
				_pollFds[j].events |= POLLOUT;
				break;
			}
		}
		
		// Move to the next client (we no longer use removeClient here)
		++i;
	}
}

//! Not needed because we already use Ainhoa's
/*
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
*/

bool Server::isCgiFd(int fd) const
{
	for (size_t i = 0; i < _clients.size(); ++i) {
		if (_clients[i]->getState() == Client::WAITING_FOR_CGI && _clients[i]->getCgiHandler() != NULL) {
			if (_clients[i]->getCgiHandler()->getReadFd() == fd || 
				_clients[i]->getCgiHandler()->getWriteFd() == fd) {
				return true;
			}
		}
	}
	return false;
}

Client* Server::getClientByCgiFd(int fd)
{
	for (size_t i = 0; i < _clients.size(); ++i) {
		if (_clients[i]->getState() == Client::WAITING_FOR_CGI && _clients[i]->getCgiHandler() != NULL) {
			if (_clients[i]->getCgiHandler()->getReadFd() == fd || 
				_clients[i]->getCgiHandler()->getWriteFd() == fd) {
				return _clients[i];
			}
		}
	}
	return NULL;
}

void Server::handleCgiEvent(size_t index)
{
	int fd = _pollFds[index].fd;
	short revents = _pollFds[index].revents;
	Client* client = getClientByCgiFd(fd);
	/*
	std::cout << "[DEBUG POLL] Evento en FD: " << fd 
          << " | revents: " << revents 
          << " (IN=" << (revents & POLLIN)
          << ", OUT=" << (revents & POLLOUT)
          << ", HUP=" << (revents & POLLHUP)
          << ", ERR=" << (revents & POLLERR) << ")\n";
	*/
	if (client == NULL) {
		close(fd);
		_pollFds.erase(_pollFds.begin() + index);
		return;
	}

	CgiHandler* cgi = client->getCgiHandler();

	//* Errors...
	if ((revents & POLLERR) || (revents & POLLNVAL) || cgi->hasError()) {
		cgi->killCgi();
		Response res = cgi->buildCgiResponse();
		client->setResponse(res.getHeadersAsString() + res.getBody());
		client->setState(Client::READY_TO_SEND);
		
		for (size_t i = 0; i < _pollFds.size(); ++i) {
			if (_pollFds[i].fd == client->getFd()) {
				_pollFds[i].events |= POLLOUT;
				break;
			}
		}
		//close(fd);  because killcgi already closes them
		_pollFds.erase(_pollFds.begin() + index);
		return;
	}

	//* If poll says we can send data to the CGI (PIPE IN)
	if ((revents & POLLOUT) && fd == cgi->getWriteFd()) {
		cgi->writeToCgi();
		if (cgi->isWriteDone() || cgi->hasError()) {
			//close(fd);
			_pollFds.erase(_pollFds.begin() + index);
			// We do not wake the client yet because we are still waiting to read from the CGI
		}
		return;
	}

	//* If there is data to read OR the CGI has closed the pipe (POLLHUP)
    if (((revents & POLLIN) || (revents & POLLHUP)) && fd == cgi->getReadFd()) {
        cgi->readFromCgi();
        
        if (cgi->isReadDone() || (revents & POLLHUP) || cgi->hasError()) {
            Response res = cgi->buildCgiResponse();
            client->setResponse(res.getHeadersAsString() + res.getBody());
            client->setState(Client::READY_TO_SEND);
            
            for (size_t i = 0; i < _pollFds.size(); ++i) {
                if (_pollFds[i].fd == client->getFd()) {
                    _pollFds[i].events |= POLLOUT;
                    break;
                }
            }
            _pollFds.erase(_pollFds.begin() + index);
        }
    }
	/*
	// If poll says the CGI has data we can read (PIPE OUT)
	if ((revents & POLLIN) && fd == cgi->getReadFd()) {
		cgi->readFromCgi();
		
		if (cgi->isReadDone() || (revents & POLLHUP) || cgi->hasError()) {
			Response res = cgi->buildCgiResponse();
			client->setResponse(res.getHeadersAsString() + res.getBody());
			client->setState(Client::READY_TO_SEND);
			
			for (size_t i = 0; i < _pollFds.size(); ++i) {
				if (_pollFds[i].fd == client->getFd()) {
					_pollFds[i].events |= POLLOUT;
					break;
				}
			}
			//close(fd);
			_pollFds.erase(_pollFds.begin() + index);
		}
	}
	*/
}