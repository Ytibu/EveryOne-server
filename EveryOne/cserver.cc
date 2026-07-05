#include "cserver.h"
#include "httpconnection.h"
#include "iocontext_pool.h"

CServer::CServer(boost::asio::io_context& ioc, unsigned short& port)
	:  _ioc(ioc), _acceptor(ioc, tcp::endpoint(tcp::v4(), port))
{
}
void CServer::start()
{
	auto self = shared_from_this();
	boost::asio::io_context &ioc = IOContextPool::GetInstance()->GetIOService();
	std::shared_ptr<HttpConnection> conn = std::make_shared<HttpConnection>(ioc);

	_acceptor.async_accept(conn->GetSocket(), [self, conn](beast::error_code ec){
		try {
			if (ec) {
				self->start();
				return;
			}

			conn->Start();

			//继续监听
			self->start();
		}
		catch(std::exception &exp){
			
		}
	});

}