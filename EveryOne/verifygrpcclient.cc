#include "verifygrpcclient.h"
#include "logger.h"


RPCConPool::RPCConPool(std::size_t pool_size, std::string host, std::string port)
    : m_isStop(false), m_poolSize(pool_size), m_host(host), m_port(port)
{
    for (std::size_t i = 0; i < m_poolSize; ++i)
    {
        auto channel = grpc::CreateChannel(m_host + ":" + m_port, grpc::InsecureChannelCredentials());
        auto stub = VerifyService::NewStub(channel);
        m_connections.push(std::move(stub));    // 入队
    }
}

RPCConPool::~RPCConPool()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    close();    // 关闭连接池，通知所有等待的线程

    // 清空连接池
    while (!m_connections.empty())
    {
        m_connections.pop();
    }
    
}

void RPCConPool::close()
{
    m_isStop = true;
    m_conVar.notify_all();
}

std::unique_ptr<VerifyService::Stub> RPCConPool::getConnection()
{
    std::unique_lock<std::mutex> lock(m_mutex);
    m_conVar.wait(lock, [this] { return !m_connections.empty() || m_isStop; });

    if (m_isStop)
    {
        return nullptr;
    }

    auto stub = std::move(m_connections.front());
    m_connections.pop();
    return stub;
}

void RPCConPool::releaseConnection(std::unique_ptr<VerifyService::Stub> stub)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if(m_isStop)
    {
        return;
    }

    m_connections.push(std::move(stub));
    m_conVar.notify_one();
}

VerifyGrpcClient::VerifyGrpcClient()
{
	auto& config = ConfigManager::GetInstance();
	std::string host = config["VerifyServer"]["host"];
	std::string port = config["VerifyServer"]["port"];
	std::string pool_size_str = config["VerifyServer"]["pool_size"];
	if (host.empty())
		host = "localhost";
	if (port.empty())
		port = "50051";
	std::size_t pool_size = pool_size_str.empty() ? 5 : std::stoul(pool_size_str);

	LOG_INFO(std::string("VerifyGrpcClient connecting to: ") + host + ":" + port
	         + " pool_size=" + std::to_string(pool_size));
	m_rpcPool.reset(new RPCConPool(pool_size, host, port));
}

GetVerifyRsp VerifyGrpcClient::GetVerifyCode(std::string email)
{
    ClientContext context;
    // 设置 5 秒超时
    context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(5));

    // RPC调用
    GetVerifyRsp reply; // 创建响应对象
    GetVerifyReq request;   // 创建请求对象

    request.set_email(email);   // 设置请求参数
    auto sub = m_rpcPool->getConnection();  // 从连接池中获取一个gRPC连接
    if (!sub)
    {
        reply.set_error(ErrorCodes::RPCFailed);
        return reply;
    }

    Status status = sub->GetVerifyCode(&context, request, &reply);  // 发起RPC调用
    m_rpcPool->releaseConnection(std::move(sub));   // 将gRPC连接释放回连接池

    if (status.ok())
    {
        LOG_INFO(std::string("VerifyGrpcClient: ") + reply.email());
    }
    else
    {
        reply.set_error(ErrorCodes::RPCFailed);
    }
    return reply;
}