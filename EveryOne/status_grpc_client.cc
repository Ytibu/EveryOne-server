#include "status_grpc_client.h"
#include "config_manager.h"
#include <grpcpp/grpcpp.h>
#include "header.h"

StatusConPool::StatusConPool(size_t poolSize, std::string host, std::string port)
    : m_poolSize(poolSize), m_host(host), m_port(port), m_isStop(false)
{
    for (size_t i = 0; i < m_poolSize; ++i)
    {
        std::shared_ptr<grpc::Channel> channel = grpc::CreateChannel(host + ":" + port, grpc::InsecureChannelCredentials());
        m_connections.push(message::StatusService::NewStub(channel));
    }
}

StatusConPool::~StatusConPool()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    close();
    while (!m_connections.empty())
    {
        m_connections.pop();
    }
}

void StatusConPool::close()
{
    m_isStop = true;
    m_conVar.notify_all();
}

std::unique_ptr<message::StatusService::Stub> StatusConPool::getConnection()
{
    std::unique_lock<std::mutex> lock(m_mutex);
    m_conVar.wait(lock, [this]
                  { return !m_connections.empty() || m_isStop; });

    if (m_isStop)
    {
        return nullptr;
    }

    auto context = std::move(m_connections.front());
    m_connections.pop();
    return context;
}

void StatusConPool::releaseConnection(std::unique_ptr<message::StatusService::Stub> stub)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_isStop)
    {
        return;
    }

    m_connections.push(std::move(stub));
    m_conVar.notify_one();
}

// ---------------------------------------------------

StatusGrpcClient::~StatusGrpcClient()
{
}
message::GetChatServerRsp StatusGrpcClient::GetChatServer(int uid)
{
    grpc::ClientContext context;
    message::GetChatServerRsp reply;
    message::GetChatServerReq request;
    request.set_uid(uid);
    auto stub = m_pool->getConnection();
    grpc::Status status = stub->GetChatServer(&context, request, &reply);
    Defer defer([&stub, this](){
        m_pool->releaseConnection(std::move(stub));
    });

    if(status.ok()){
        return reply;
    }else{
        reply.set_error(ErrorCodes::RPCFailed);
        return reply;
    }
    
}
message::LoginRsp StatusGrpcClient::Login(int uid, std::string token)
{
}

StatusGrpcClient::StatusGrpcClient()
{
    auto &Config = ConfigManager::GetInstance();
    std::string host = Config["StatusServer"]["host"];
    std::string port = Config["StatusServer"]["port"];

    m_pool.reset(new StatusConPool(5, host, port));
}