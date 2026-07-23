#include "logicsystem.h"
#include "httpconnection.h"
#include <shared_mutex>
#include <jsoncpp/json/json.h>
#include "verifygrpcclient.h"
#include "status_grpc_client.h"
#include "logger.h"
#include "redismgr.h"
#include "mysql_mgr.h"
#include "header.h"

#define CODEPREFIX "code_"


LogicSystem::LogicSystem()
{
	RegGet("/get_test", [](std::shared_ptr<HttpConnection> conn) {
		conn->AppendResponseBody("receive get_test req");
		int i = 0;
		for (auto& [key, value] : conn->GetParams()) {
			conn->AppendResponseBody("param " + std::to_string(i++) + ": " + key + "=" + value + "\r\n");
		}
	});

	RegPost("/get_verify", [](std::shared_ptr<HttpConnection> conn) {
		auto body = conn->GetRequestBody();
		LOG_INFO(std::string("receive get_verify req, body: ") + body); 
		conn->SetContentType("application/json");
		Json::Value root;	// 创建一个 JSON 根对象
		Json::Reader reader;	// 创建一个 JSON 解析器
		Json::Value src_root;

		bool parsingSuccessful = reader.parse(body, src_root);
		if (!parsingSuccessful)
		{
			LOG_WARN(std::string("Failed to parse JSON: ") + reader.getFormattedErrorMessages());
			root["error"] = ErrorCodes::Error_Json;
			conn->SetStatusCode(http::status::bad_request);
			conn->AppendResponseBody("Failed to parse JSON: " + reader.getFormattedErrorMessages());
			return;
		}

		if(!src_root.isMember("email"))
		{
			LOG_WARN("Missing 'email' field in JSON request");
			root["error"] = ErrorCodes::Error_Json;
			conn->SetStatusCode(http::status::bad_request);
			conn->AppendResponseBody("Missing 'email' field in JSON request");
			return;
		}

		auto email = src_root["email"].asString();
		auto verify_rsp = VerifyGrpcClient::GetInstance().GetVerifyCode(email);
		LOG_INFO(std::string("email: ") + email);
		root["error"] = verify_rsp.error();
		root["email"] = verify_rsp.email();
		std::string responseBody = root.toStyledString();
		conn->AppendResponseBody(responseBody);
		return; 
	});

	RegPost("/user_register", [](std::shared_ptr<HttpConnection> connection) {

		// 1. 获取请求体
		auto body_str = connection->GetRequestBody();
		LOG_INFO(std::string("receive user_register req, body: ") + body_str);
		
		// 2. 设置响应内容类型为 JSON
		connection->SetContentType("application/json");

		// 3. 解析 JSON 请求体
		Json::Value root;
		Json::Reader reader;
		Json::Value src_root;
		bool parse_success = reader.parse(body_str, src_root);
		if (!parse_success) {
			LOG_WARN("Failed to parse JSON data!");
			root["error"] = ErrorCodes::Error_Json;
			std::string jsonstr = root.toStyledString();
			connection->AppendResponseBody(jsonstr);
			return true;
		}

		// 4. 检查必要字段是否存在
		auto email = src_root["email"].asString();
		auto name = src_root["user"].asString();
		auto pwd = src_root["password"].asString();
		auto confirm = src_root["confirm"].asString();

		if (pwd != confirm) {
			std::cout << "password err " << std::endl;
			root["error"] = ErrorCodes::PasswdErr;
			std::string jsonstr = root.toStyledString();
			connection->AppendResponseBody(jsonstr);
			return true;
		}

		//先查找redis中email对应的验证码是否合理
		std::string  varify_code;
		bool b_get_varify = RedisMgr::GetInstance().Get(CODEPREFIX+src_root["email"].asString(), varify_code);
		if (!b_get_varify) {
			LOG_WARN("get varify code expired");
			root["error"] = ErrorCodes::VarifyExpired;
			std::string jsonstr = root.toStyledString();
			connection->AppendResponseBody(jsonstr);
			return true;
		}

		std::string request_varify_code = src_root["verifycode"].asString();
		if (request_varify_code.empty() || varify_code != request_varify_code) {
			std::cout << " varify code error" << std::endl;
			root["error"] = ErrorCodes::VarifyCodeErr;
			std::string jsonstr = root.toStyledString();
			connection->AppendResponseBody(jsonstr);
			return true;
		}

		//查找数据库判断用户是否存在
		int uid = MysqlMgr::GetInstance().registerUser(name, email, pwd);
		if (uid == 0 || uid == -1) {
			std::cout << " user or email exist" << std::endl;
			root["error"] = ErrorCodes::UserExist;
			std::string jsonstr = root.toStyledString();
			connection->AppendResponseBody(jsonstr);
			return true;
		}
		root["error"] = 0;
		root["uuid"] = uid;
		root["email"] = email;
		root ["user"]= name;
		root ["password"]= pwd;
		root ["confirm"]= confirm;
		root["verifycode"] = request_varify_code;
		std::string jsonstr = root.toStyledString();
		connection->AppendResponseBody(jsonstr);
		return true; 
	});

	RegPost("/reset_pwd", [](std::shared_ptr<HttpConnection> connection) {
		// 获取收到的内容
		auto body_str = connection->GetRequestBody();
		LOG_INFO(std::string("receive reset_pwd req, body: ") + body_str);
		
		// 设置响应头
		connection->SetContentType("application/json");
		
		//  解析内容
		Json::Value root;
		Json::Reader reader;
		Json::Value src_root;
		bool parse_success = reader.parse(body_str, src_root);
		if (!parse_success) {
			LOG_WARN("Failed to parse JSON data!");
			root["error"] = ErrorCodes::Error_Json;
			std::string jsonstr = root.toStyledString();
			connection->AppendResponseBody(jsonstr);
			return true;
		}

		auto email = src_root["email"].asString();
		auto name = src_root["user"].asString();
		auto pwd = src_root["password"].asString();

		// 获取redis中的验证码，并校验验证码是否正确
		std::string  varify_code;
		bool b_get_varify = RedisMgr::GetInstance().Get(CODEPREFIX + src_root["email"].asString(), varify_code);
		if (!b_get_varify) {
			LOG_WARN("get varify code expired");
			root["error"] = ErrorCodes::VarifyExpired;
			std::string jsonstr = root.toStyledString();
			connection->AppendResponseBody(jsonstr);
			return true;
		}

		std::string request_varify_code = src_root["verifycode"].asString();
		if (request_varify_code.empty() || varify_code != request_varify_code) {
			LOG_WARN("varify code error");
			root["error"] = ErrorCodes::VarifyCodeErr;
			std::string jsonstr = root.toStyledString();
			connection->AppendResponseBody(jsonstr);
			return true;
		}

		// 判断mysql中用户名与邮箱是否匹配
		bool b_check_email = MysqlMgr::GetInstance().checkEmail(name, email);
		if (!b_check_email) {
			LOG_WARN("email not match");
			root["error"] = ErrorCodes::EmailNotMatch;
			std::string jsonstr = root.toStyledString();
			connection->AppendResponseBody(jsonstr);
			return true;
		}

		// 更新密码
		bool b_update_pwd = MysqlMgr::GetInstance().updatePassword(name, pwd);
		if (!b_update_pwd) {
			LOG_WARN("update password failed");
			root["error"] = ErrorCodes::PasswdUpFailed;
			std::string jsonstr = root.toStyledString();
			connection->AppendResponseBody(jsonstr);
			return true;
		}

		root["error"] = 0;
		root["email"] = email;
		root["user"] = name;
		root["password"] = pwd;
		root["verifycode"] = request_varify_code;
		std::string jsonstr = root.toStyledString();
		LOG_INFO("root is" + jsonstr);
		connection->AppendResponseBody(jsonstr);
		return true;
	});

	RegPost("/user_login", [](std::shared_ptr<HttpConnection> connection) {
		// 获取收到的内容
		auto body_str = connection->GetRequestBody();
		LOG_INFO(std::string("receive user_login req, body: ") + body_str);
		
		// 设置响应头
		connection->SetContentType("application/json");
		
		//  解析内容
		Json::Value root;
		Json::Reader reader;
		Json::Value src_root;
		bool parse_success = reader.parse(body_str, src_root);
		if (!parse_success) {
			LOG_WARN("Failed to parse JSON data!");
			root["error"] = ErrorCodes::Error_Json;
			std::string jsonstr = root.toStyledString();
			connection->AppendResponseBody(jsonstr);
			return true;
		}

		auto email = src_root["email"].asString();
		auto pwd = src_root["password"].asString();

		// 判断用户名和密码是否正确
		UserInfo userInfo;
		bool b_check_pwd = MysqlMgr::GetInstance().checkPassword(email, pwd, userInfo);
		if (!b_check_pwd) {
			LOG_WARN("username or password error");
			root["error"] = ErrorCodes::PasswdErr;
			std::string jsonstr = root.toStyledString();
			connection->AppendResponseBody(jsonstr);
			return true;
		}

		// 查询StatusServer获取用户状态
		auto reply = StatusGrpcClient::GetInstance().GetChatServer(userInfo.uid);
		if(reply.error()){
			LOG_WARN("get chat server failed");
			root["error"] = ErrorCodes::RPCFailed;
			std::string jsonstr = root.toStyledString();
			connection->AppendResponseBody(jsonstr);
			return true;
		}

		root["error"] = 0;
		root["uuid"] = userInfo.uid;
		root["email"] = email;
		root["token"] = reply.token();
		root["host"] = reply.host();
		root["port"] = reply.port();
		std::string jsonstr = root.toStyledString();
		LOG_INFO("root is" + jsonstr);
		connection->AppendResponseBody(jsonstr);
		return true;
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