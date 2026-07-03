#include "httpconnection.h"
#include <iostream>
#include "logicsystem.h"

HttpConnection::HttpConnection(tcp::socket socket)
	: _socket(std::move(socket))
{
}

unsigned char toHex(unsigned char x)
{
	return x > 9 ? x + 55 : x + 48;
}

unsigned char fromHex(unsigned char x)
{
	unsigned char y;
	if (x >= 'A' && x <= 'Z')
		y = x - 'A' + 10;
	else if (x >= 'a' && x <= 'z')
		y = x - 'a' + 10;
	else if (x >= '0' && x <= '9')
		y = x - '0';
	else
		assert(0);
	return y;
}

std::string urlDecode(const std::string &str)
{
	std::string strTemp = "";
	size_t length = str.length();
	for (size_t i = 0; i < length; i++)
	{
		if (str[i] == '+')
			strTemp += ' ';
		else if (str[i] == '%')
		{
			assert(i + 2 < length);
			unsigned char high = fromHex((unsigned char)str[++i]);
			unsigned char low = fromHex((unsigned char)str[++i]);
			strTemp += high * 16 + low;
		}
		else
			strTemp += str[i];
	}
	return strTemp;
}

std::string urlEncode(const std::string &str)
{
	std::string strTemp = "";
	size_t length = str.length();
	for (size_t i = 0; i < length; i++)
	{
		if (isalnum((unsigned char)str[i]) ||
			(str[i] == '-') ||
			(str[i] == '_') ||
			(str[i] == '.') ||
			(str[i] == '~'))
			strTemp += str[i];
		else if (str[i] == ' ')
			strTemp += "+";
		else
		{
			strTemp += '%';
			strTemp += toHex((unsigned char)str[i] >> 4);
			strTemp += toHex((unsigned char)str[i] % 0x0F);
		}
	}
	return strTemp;
}

void HttpConnection::Start()
{
	auto self = shared_from_this();
	http::async_read(_socket, _buffer, _request, [self](beast::error_code ec, std::size_t bytes_transferred)
					 {
		try {
			if (ec) {
				return;
			}
			boost::ignore_unused(bytes_transferred);
			self->handleReq();
			self->CheckDeadline();
		}catch (std::exception& exp) {
			std::cout << "HttpConnection::Start exception: " << exp.what() << std::endl;
		} });
}

void HttpConnection::CheckDeadline()
{
	auto self = shared_from_this();
	_dealine.async_wait([self](beast::error_code ec)
						{
		if (!ec) {
			self->_socket.close();
		} });
}
void HttpConnection::WirteResponse()
{
	auto self = shared_from_this();
	_response.content_length(_response.body().size());
	http::async_write(_socket, _response, [self](beast::error_code ec, std::size_t bytes_transferred)
					  {
		self->_socket.shutdown(tcp::socket::shutdown_send, ec);
		self->_dealine.cancel(); });
}

void HttpConnection::handleReq()
{
	_response.version(_request.version());
	_response.keep_alive(false);
	if (_request.method() == http::verb::get)
	{
		PreParseGetParam();
		bool success = LogicSystem::GetInstance()->HandleGet(_get_url, shared_from_this());
		if (!success)
		{
			_response.result(http::status::not_found);
			_response.set(http::field::content_type, "text/plain");
			beast::ostream(_response.body()) << "url not found\r\n";
			WirteResponse();
			return;
		}

		_response.result(http::status::ok);
		_response.set(http::field::server, "GateServer");
		WirteResponse();
		return;
	}

	if(_request.method() == http::verb::post)
	{
		bool success = LogicSystem::GetInstance()->HandlePost(_request.target(), shared_from_this());
		if (!success)
		{
			_response.result(http::status::not_found);
			_response.set(http::field::content_type, "text/plain");
			beast::ostream(_response.body()) << "url not found\r\n";
			WirteResponse();
			return;
		}

		_response.result(http::status::ok);
		_response.set(http::field::server, "GateServer");
		WirteResponse();
		return;
	}
}

// 解析get请求参数
void HttpConnection::PreParseGetParam()
{
	auto uri = _request.target();
	auto queryPos = uri.find('?');
	if (queryPos == std::string::npos)
	{
		_get_url = uri;
		return;
	}

	_get_url = uri.substr(0, queryPos);
	std::string queryStr = uri.substr(queryPos + 1);
	std::string key, value;
	size_t pos = 0;
	while (pos < queryStr.size())
	{
		size_t equalPos = queryStr.find('=', pos);
		if (equalPos == std::string::npos)
		{
			break;
		}
		key = queryStr.substr(pos, equalPos - pos);
		pos = equalPos + 1;
		size_t ampPos = queryStr.find('&', pos);
		if (ampPos == std::string::npos)
		{
			value = queryStr.substr(pos);
			pos = queryStr.size();
		}
		else
		{
			value = queryStr.substr(pos, ampPos - pos);
			pos = ampPos + 1;
		}

		key = urlDecode(key);
		value = urlDecode(value);
		_get_param[key] = value;
	}
}