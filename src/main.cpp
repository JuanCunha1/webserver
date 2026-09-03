#include <iostream>

#include "network/Server.hpp"
#include <iostream>
#include <exception>

int main()
{
	try
	{
		Server server;
		std::vector<int> port;
		port.push_back(8080);
		port.push_back(8081);
		server.start(port);
		server.run();
	}
	catch (const std::exception &e)
	{
		std::cerr << e.what() << std::endl;
		return 1;
	}

	return 0;
}