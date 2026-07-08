#include "logicsystem.h"
#include "httpconnection.h"
#include <shared_mutex>
#include <jsoncpp/json/json.h>
#include "verifygrpcclient.h"
#include "logger.h"
#include "redismgr.h"

LogicSystem::LogicSystem()
{
	RegGet("/get_test", [](std::shared_ptr<HttpConnection> conn) {
		conn->AppendResponseBody("receive get_test req");
		int i = 0;
		for (auto& [key, value] : conn->GetParams()) {
			conn->AppendResponseBody("param " + std::to_string(i++) + ": " + key + "=" + value + "\r\n");
		}
	});

	RegPost("/post_test", [](std::shared_ptr<HttpConnection> conn){
		auto body = conn->GetRequestBody();
		LOG_INFO(std::string("receive post_test req, body: ") + body); 
		conn->SetContentType("text/json");
		Json::Value root;	// 创建一个 JSON 根对象
		Json::Reader reader;	// 创建一个 JSON 解析器
		Json::Value data;	// 用于存储解析后的数据
		bool parsingSuccessful = reader.parse(body, root);
		if (!parsingSuccessful)
		{
			LOG_WARN(std::string("Failed to parse JSON: ") + reader.getFormattedErrorMessages());
			conn->SetStatusCode(http::status::bad_request);
			conn->AppendResponseBody("Failed to parse JSON: " + reader.getFormattedErrorMessages());
			return;
		}

		if(!root.isMember("email"))
		{
			LOG_WARN("Missing 'email' field in JSON request");
			conn->SetStatusCode(http::status::bad_request);
			conn->AppendResponseBody("Missing 'email' field in JSON request");
			return;
		}

		auto email = root["email"].asString();
		auto verify_rsp = VerifyGrpcClient::GetInstance().GetVerifyCode(email);
		LOG_INFO(std::string("email: ") + email);
		root["error"] = verify_rsp.error();
		root["email"] = verify_rsp.email();
		root["code"] = verify_rsp.code();
		std::string responseBody = root.toStyledString();
		conn->AppendResponseBody(responseBody);
		return; 
	});

	RegPost("/user_register", [](std::shared_ptr<HttpConnection> conn) {
		auto body = conn->GetRequestBody();
		LOG_INFO(std::string("receive user_register req, body: ") + body); 
		conn->SetContentType("text/json");

		Json::Value root;	// 创建一个 JSON 根对象
		Json::Reader reader;	// 创建一个 JSON 解析器
		Json::Value response;	// 用于存储解析后的数据

		bool parsingSuccessful = reader.parse(body, root);
		if (!parsingSuccessful)
		{
			LOG_WARN(std::string("Failed to parse JSON: ") + reader.getFormattedErrorMessages());
			conn->SetStatusCode(http::status::bad_request);
			response["error"] = "Invalid JSON format";
			conn->AppendResponseBody(response.toStyledString());
			return;
		}


		// redis查询验证码是否过期
		std::string verify_code;
		bool get_verify = RedisMgr::GetInstance().Get(root["email"].asString(), verify_code);
		if(!get_verify)
		{
			LOG_WARN("Verification code expired or not found in Redis");
			conn->SetStatusCode(http::status::bad_request);
			response["error"] = "Verification code expired or not found";
			conn->AppendResponseBody(response.toStyledString());
			return;
		}

		root["error"] = 0;
		root["email"] = response["email"];
		root["user"] = response["user"];
		root["passwd"] = response["passwd"];
		root["confirm"] = response["confirm"];
		root["verifyCode"] = response["verifyCode"];
		std::string responseBody = root.toStyledString();
		conn->AppendResponseBody(responseBody);
		return;
	});
}

LogicSystem::~LogicSystem()
{
}

bool LogicSystem::HandleGet(const std::string &path, std::shared_ptr<HttpConnection> conn)
{
	std::shared_lock lock(_get_mutex);
	auto it = _get_handlers.find(path);
	if (it == _get_handlers.end())
	{
		return false;
	}
	it->second(conn);
	return true;
}

bool LogicSystem::HandlePost(const std::string &path, std::shared_ptr<HttpConnection> conn)
{
	std::shared_lock lock(_post_mutex);
	auto it = _post_handlers.find(path);
	if (it == _post_handlers.end())
	{
		return false;
	}
	it->second(conn);
	return true;
}

void LogicSystem::RegGet(const std::string &path, HttpHandler handler)
{
	std::unique_lock lock(_get_mutex);
	_get_handlers.insert(std::make_pair(path, handler));
}
void LogicSystem::RegPost(const std::string &path, HttpHandler handler)
{
	std::unique_lock lock(_post_mutex);
	_post_handlers.insert(std::make_pair(path, handler));
}