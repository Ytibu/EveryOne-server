#include "httpconnection.h"
#include "logicsystem.h"
#include "logger.h"

HttpConnection::HttpConnection(boost::asio::io_context& ioc)
	: m_socket(ioc)
{
}

void HttpConnection::AppendResponseBody(const std::string& content)
{
	beast::ostream(m_response.body()) << content;
}

void HttpConnection::SetStatusCode(http::status code)
{
	m_response.result(code);
}

void HttpConnection::SetContentType(const std::string& type)
{
	m_response.set(http::field::content_type, type);
}

std::string HttpConnection::GetRequestBody() const
{
	return beast::buffers_to_string(m_request.body().data());
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
		return 0;
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
			if (i + 2 >= length)
				return strTemp;
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

void HttpConnection::start()
{
	auto self = shared_from_this();
	http::async_read(m_socket, m_buffer, m_request, [self](beast::error_code ec, std::size_t bytes_transferred) {
		try {
			if (ec) {
				return;
			}
			boost::ignore_unused(bytes_transferred);
			self->CheckDeadline();
			self->handleReq();
		}catch (std::exception& exp) {
			LOG_ERROR(std::string("HttpConnection::Start exception: ") + exp.what());
		} 
	});
}

void HttpConnection::CheckDeadline()
{
	auto self = shared_from_this();
	m_deadline.async_wait([self](beast::error_code ec) {
		if (!ec) {
			self->m_socket.close();
		} 
	});
}
void HttpConnection::WriteResponse()
{
	auto self = shared_from_this();
	m_response.content_length(m_response.body().size());
	http::async_write(m_socket, m_response, [self](beast::error_code ec, std::size_t bytes_transferred) {
		self->m_socket.shutdown(tcp::socket::shutdown_send, ec);
		self->m_deadline.cancel(); 
	});
}

void HttpConnection::handleReq()
{
	m_response.version(m_request.version());
	m_response.keep_alive(false);
	if (m_request.method() == http::verb::get)
	{
		PreParseGetParam();
		bool success = LogicSystem::GetInstance().HandleGet(m_get_url, shared_from_this());
		if (!success)
		{
			m_response.result(http::status::not_found);
			m_response.set(http::field::content_type, "text/plain");
			beast::ostream(m_response.body()) << "url not found\r\n";
			WriteResponse();
			return;
		}

		m_response.result(http::status::ok);
		m_response.set(http::field::server, "GateServer");
		WriteResponse();
		return;
	}

	if(m_request.method() == http::verb::post)
	{
		bool success = LogicSystem::GetInstance().HandlePost(m_request.target(), shared_from_this());
		if (!success)
		{
			m_response.result(http::status::not_found);
			m_response.set(http::field::content_type, "text/plain");
			beast::ostream(m_response.body()) << "url not found\r\n";
			WriteResponse();
			return;
		}

		m_response.result(http::status::ok);
		m_response.set(http::field::server, "GateServer");
		WriteResponse();
		return;
	}
}

// 解析get请求参数
void HttpConnection::PreParseGetParam()
{
	auto uri = m_request.target();
	auto queryPos = uri.find('?');
	if (queryPos == std::string::npos)
	{
		m_get_url = uri;
		return;
	}

	m_get_url = uri.substr(0, queryPos);
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
		m_get_param[key] = value;
	}
}