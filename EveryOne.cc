#include "EveryOne/cserver.h"
#include "EveryOne/config_manager.h"
#include "EveryOne/iocontext_pool.h"
#include "EveryOne/logger.h"

int main()
{
	std::string port_str = ConfigManager::GetInstance()["GateServer"]["port"];	// 从配置文件中获取端口号
	unsigned short port = static_cast<unsigned short>(std::stoi(port_str));	// 转换端口号类型
	
	try
	{
		boost::asio::io_context ioc{1};
		boost::asio::signal_set signals(ioc, SIGINT, SIGTERM);
		signals.async_wait([&ioc](beast::error_code const &error, int signal_number) {
			if (error) {
				return;
			}
			ioc.stop(); 
		});

		std::shared_ptr<CServer> cserver = std::make_shared<CServer>(ioc, port);
		cserver->start();
		
		ioc.run();
	}
	catch (std::exception const &exc)
	{
		LOG_ERROR(std::string("Error: ") + exc.what());
		return EXIT_FAILURE;
	}
	
	return 0;
}
