#include <iostream>
#include "cserver.h"
#include "config_manager.h"

int main()
{
	ConfigManager config;
	std::string gateway_ip = config["GateServer"]["port"];
	unsigned short port = static_cast<unsigned short>(std::stoi(gateway_ip));
	try {
		boost::asio::io_context ioc{ 1 };
		boost::asio::signal_set signals(ioc, SIGINT, SIGTERM);
		signals.async_wait([&ioc](beast::error_code const& error, int signal_number) {
			if (error) {
				return;
			}
			ioc.stop();
		});
		
		std::make_shared<CServer>(ioc, port)->start();
		ioc.run();
	}
	catch(std::exception const &exc){
		std::cerr << "Error: " << exc.what() << std::endl;
		return EXIT_FAILURE;
	}

	return 0;
}
