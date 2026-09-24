#include "config/ConfigParser.hpp"
#include "protocol/RequestParser.hpp"
#include "protocol/ResponseBuilder.hpp"
#include "network/Server.hpp"
#include <iostream>
#include <exception>

int main(int argc, char *argv[])
{
	if(argc != 2)
	{
		std::cerr << "Usage: " << argv[0] << " <config_file>" << std::endl;
		return 1;
	}

	try
	{
		ConfigParser parser;
		parser.parseFile(argv[1]);
		Server server(parser.getServers());
		server.start();
		server.run();
	}
	catch (const std::exception &e)
	{
		std::cerr << e.what() << std::endl;
		return 1;
	}

	return 0;
}