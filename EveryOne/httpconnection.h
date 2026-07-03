#pragma once
#include <memory>
#include "header.h"

class HttpConnection : public std::enable_shared_from_this<HttpConnection>
{
public:
	friend class LogicSystem;
	HttpConnection(tcp::socket socket);
	void Start();

private:
	void CheckDeadline();
	void WirteResponse();
	void handleReq();
	void PreParseGetParam();// 预解析get请求参数

private:
	tcp::socket _socket;
	beast::flat_buffer _buffer{ 8196 };
	http::request<http::dynamic_body> _request;
	http::response<http::dynamic_body> _response;

	net::steady_timer _dealine{ _socket.get_executor(), std::chrono::seconds(60) };

	std::string _get_url;
	std::unordered_map<std::string, std::string> _get_param;
};