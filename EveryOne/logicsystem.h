#pragma once

#include <functional>
#include <memory>
#include <map>
#include <string>
#include "singleton.h"

class HttpConnection;

using HttpHandler = std::function<void(std::shared_ptr<HttpConnection>)>;

class LogicSystem : public Singleton<LogicSystem>
{
friend class Singleton<LogicSystem>;
public:
	~LogicSystem();
	bool HandleGet(const std::string& path, std::shared_ptr<HttpConnection> conn);
	bool HandlePost(const std::string& path, std::shared_ptr<HttpConnection> conn);
	void RegGet(const std::string& path, HttpHandler handler);
	void RegPost(const std::string& path, HttpHandler handler);

private:
	LogicSystem();

private:
	std::map<std::string, HttpHandler> _post_handlers;	// post请求处理函数
	std::map<std::string, HttpHandler> _get_handlers;	// get请求处理函数
};