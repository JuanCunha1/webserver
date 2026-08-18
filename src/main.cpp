#include <iostream>

#include "network/Server.hpp"
#include <iostream>
#include <exception>

int main()
{
	try
	{
		Server server(8080);

		server.start();

		while (true)
		{
			server.acceptClient();
		}
	}
	catch (const std::exception &e)
	{
		std::cerr << e.what() << std::endl;
		return 1;
	}

	return 0;
}