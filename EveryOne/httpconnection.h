#pragma once
#include <memory>
#include "header.h"

class HttpConnection : public std::enable_shared_from_this<HttpConnection>
{
public:
	HttpConnection(boost::asio::io_context &ioc);
	void start();
	tcp::socket &getSocket() { return m_socket; }

	// 公开访问接口 —— 替代 friend class LogicSystem 直接访问私有成员
	void AppendResponseBody(const std::string &content);
	void SetStatusCode(http::status code);
	void SetContentType(const std::string &type);
	std::string GetRequestBody() const;
	const std::unordered_map<std::string, std::string> &GetParams() const { return m_get_param; }

private:
	void CheckDeadline();
	void WriteResponse();
	void handleReq();
	void PreParseGetParam(); // 预解析get请求参数

private:
	tcp::socket m_socket;
	beast::flat_buffer m_buffer{8196};
	http::request<http::dynamic_body> m_request;
	http::response<http::dynamic_body> m_response;

	net::steady_timer m_deadline{m_socket.get_executor(), std::chrono::seconds(60)};

	std::string m_get_url;
	std::unordered_map<std::string, std::string> m_get_param;
};