#include "cserver.h"
#include "httpconnection.h"

CServer::CServer(boost::asio::io_context& ioc, unsigned short& port)
	: _acceptor(ioc, tcp::endpoint(tcp::v4(), port)), _ioc(ioc), _socket(ioc)
{

}
void CServer::start()
{
	auto self = shared_from_this();
	_acceptor.async_accept(_socket, [self](beast::error_code ec){
		try {
			if (ec) {
				self->start();
				return;
			}
			// 创建连接，并创建HttpConnection类管理此连接
			std::make_shared<HttpConnection>(std::move(self->_socket))->Start();

			//继续监听
			self->start();
		}
		catch(std::exception &exp){
			
		}
	});

}