#include <iostream>
#include "cserver.h"

int main()
{
	try {
		unsigned short port = static_cast<unsigned short>(8080);
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
