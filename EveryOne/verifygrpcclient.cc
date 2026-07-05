#include "verifygrpcclient.h"

VerifyGrpcClient::VerifyGrpcClient()
{
    ConfigManager config;
    std::string host = config["VerifyServer"]["host"];
    std::string port = config["VerifyServer"]["port"];
    if (host.empty())
        host = "localhost";
    if (port.empty())
        port = "50051";

    std::string server_address = host + ":" + port;
    std::cout << "VerifyGrpcClient connecting to: " << server_address << std::endl;
    // stub_ = VerifyService::NewStub(grpc::CreateChannel(server_address, grpc::InsecureChannelCredentials()));
    m_rpcPool.reset(new RPCConPool(5, host, port));
}

GetVerifyRsp VerifyGrpcClient::GetVerifyCode(std::string email)
{
    ClientContext context;
    // 设置 5 秒超时
    context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(5));

    GetVerifyRsp reply;
    GetVerifyReq request;
    request.set_email(email);
    auto sub = m_rpcPool->getConnection();
    Status status = sub->GetVerifyCode(&context, request, &reply);
    m_rpcPool->releaseConnection(std::move(sub));
    if (status.ok())
    {
        std::cout << "VerifyGrpcClient: " << reply.email() << std::endl;
    }
    else
    {
        reply.set_error(ErrorCodes::RPCFailed);
    }
    return reply;
}

RPCConPool::RPCConPool(std::size_t pool_size, std::string host, std::string port)
    : m_isStop(false), m_poolSize(pool_size), m_host(host), m_port(port)
{
    for (std::size_t i = 0; i < m_poolSize; ++i)
    {
        auto channel = grpc::CreateChannel(m_host + ":" + m_port, grpc::InsecureChannelCredentials());
        auto stub = VerifyService::NewStub(channel);
        m_connections.push_back(std::move(stub));
    }
}

RPCConPool::~RPCConPool()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    close();
    while (!m_connections.empty())
    {
        m_connections.pop_back();
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

    auto stub = std::move(m_connections.back());
    m_connections.pop_back();
    return stub;
}

void RPCConPool::releaseConnection(std::unique_ptr<VerifyService::Stub> stub)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if(m_isStop)
    {
        return;
    }

    m_connections.push_back(std::move(stub));
    m_conVar.notify_one();
}
