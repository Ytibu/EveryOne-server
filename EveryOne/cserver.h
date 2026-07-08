#pragma once
#include "header.h"

class CServer : public std::enable_shared_from_this<CServer>
{
public:
	CServer(boost::asio::io_context& ioc, unsigned short port);
	void start();

private:
	tcp::acceptor m_acceptor;
	boost::asio::io_context& m_ioc;
};