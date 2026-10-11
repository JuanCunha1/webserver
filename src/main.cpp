#include "config/ConfigParser.hpp"
#include "protocol/RequestParser.hpp"
#include "protocol/ResponseBuilder.hpp"
#include "network/Server.hpp"
#include <iostream>
#include <exception>
#include <signal.h>

volatile sig_atomic_t g_running = 1;

void handle_sigint(int signum)
{
    (void)signum;
    std::cout << "\n[INFO] Apagando el servidor de forma segura..." << std::endl;
    g_running = false;
}

int main(int argc, char *argv[])
{
	if(argc != 2)
	{
		std::cerr << "Usage: " << argv[0] << " <config_file>" << std::endl;
		return 1;
	}

	signal(SIGINT, handle_sigint);
	try
	{
		signal(SIGPIPE, SIG_IGN);
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