#include "logicsystem.h"
#include "httpconnection.h"


LogicSystem::~LogicSystem()
{

}

bool LogicSystem::HandleGet(const std::string& path, std::shared_ptr<HttpConnection> conn)
{
	if (_get_handlers.find(path) == _get_handlers.end())
	{
		return false;
	}
	_get_handlers[path](conn);
	return true;
}

void LogicSystem::RegGet(const std::string& path, HttpHandler handler)
{
	_get_handlers.insert(std::make_pair(path, handler));
}

LogicSystem::LogicSystem()
{
	RegGet("/test", [](std::shared_ptr<HttpConnection> conn) {
		beast::ostream(conn->_response.body()) << "receive get_test req";
	});
}