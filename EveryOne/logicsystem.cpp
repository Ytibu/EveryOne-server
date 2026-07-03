#include "logicsystem.h"
#include "httpconnection.h"
#include <iostream>
#include <jsoncpp/json/json.h>
#include "verifygrpcclient.h"

LogicSystem::LogicSystem()
{
	RegGet("/get_test", [](std::shared_ptr<HttpConnection> conn) {
		beast::ostream(conn->_response.body()) << "receive get_test req";
		int i = 0;
		for (auto& [key, value] : conn->_get_param) {
			beast::ostream(conn->_response.body()) << "param " << i++ << ": " << key << "=" << value << "\r\n";
		}
	});

	RegPost("/post_test", [](std::shared_ptr<HttpConnection> conn){
		auto body = boost::beast::buffers_to_string(conn->_request.body().data());
		std::cout << "receive post_test req, body: " << body << std::endl; 
		conn->_response.set(http::field::content_type, "text/json");
		Json::Value root;	// 创建一个 JSON 根对象
		Json::Reader reader;	// 创建一个 JSON 解析器
		Json::Value data;	// 用于存储解析后的数据
		bool parsingSuccessful = reader.parse(body, root);
		if (!parsingSuccessful)
		{
			std::cout << "Failed to parse JSON: " << reader.getFormattedErrorMessages();
			conn->_response.result(http::status::bad_request);
			beast::ostream(conn->_response.body()) << "Failed to parse JSON: " << reader.getFormattedErrorMessages();
			return;
		}

		if(!root.isMember("email"))
		{
			std::cout << "Missing 'email' field in JSON request" << std::endl;
			conn->_response.result(http::status::bad_request);
			beast::ostream(conn->_response.body()) << "Missing 'email' field in JSON request";
			return;
		}

		auto email = root["email"].asString();
		auto verify_rsp = VerifyGrpcClient::GetInstance()->GetVerifyCode(email);
		std::cout << "email: " << email << std::endl;
		root["error"] = verify_rsp.error();
		root["email"] = verify_rsp.email();
		std::string responseBody = root.toStyledString();
		beast::ostream(conn->_response.body()) << responseBody;
		return; 
	});
}

LogicSystem::~LogicSystem()
{
}

bool LogicSystem::HandleGet(const std::string &path, std::shared_ptr<HttpConnection> conn)
{
	if (_get_handlers.find(path) == _get_handlers.end())
	{
		return false;
	}
	_get_handlers[path](conn);
	return true;
}

bool LogicSystem::HandlePost(const std::string &path, std::shared_ptr<HttpConnection> conn)
{
	if (_post_handlers.find(path) == _post_handlers.end())
	{
		return false;
	}
	_post_handlers[path](conn);
	return true;
}

void LogicSystem::RegGet(const std::string &path, HttpHandler handler)
{
	_get_handlers.insert(std::make_pair(path, handler));
}
void LogicSystem::RegPost(const std::string &path, HttpHandler handler)
{
	_post_handlers.insert(std::make_pair(path, handler));
}