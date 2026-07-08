#include "cserver.h"

#include "httpconnection.h"
#include "iocontext_pool.h"
#include "logger.h"

CServer::CServer(boost::asio::io_context& ioc, unsigned short port)
	:  m_ioc(ioc), m_acceptor(ioc, tcp::endpoint(tcp::v4(), port))
{
}

void CServer::start()
{
	auto self = shared_from_this();
	boost::asio::io_context& pool_ioc = IOContextPool::GetInstance().GetIOService();
	std::shared_ptr<HttpConnection> conn = std::make_shared<HttpConnection>(pool_ioc);

	m_acceptor.async_accept(conn->getSocket(), [self, conn](beast::error_code ec){
		try {
			if (ec) {
				self->start();
				return;
			}

			conn->start();

			//继续监听
			self->start();
		}
		catch(std::exception &exp){
			LOG_ERROR(std::string("CServer::start() exception: ") + exp.what());
		}
	});

}