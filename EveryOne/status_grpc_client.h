#pragma once
#include <string>
#include <cstddef>
#include <queue>
#include <atomic>
#include <memory>
#include <condition_variable>
#include <mutex>

#include "message.pb.h"
#include "message.grpc.pb.h"
#include "singleton.h"

class StatusConPool
{
public:
    StatusConPool(size_t poolSize, std::string host, std::string port);
    ~StatusConPool();

    void close();

    std::unique_ptr<message::StatusService::Stub> getConnection();
    void releaseConnection(std::unique_ptr<message::StatusService::Stub> stub);

private:
    std::size_t m_poolSize;
    std::string m_host;
    std::string m_port;
    std::atomic<bool> m_isStop;
    std::queue<std::unique_ptr<message::StatusService::Stub>> m_connections; // 存储gRPC连接的连接池
    std::condition_variable m_conVar;
    std::mutex m_mutex;
};

class StatusGrpcClient : public Singleton<StatusGrpcClient>
{
    friend class Singleton<StatusGrpcClient>;
public:
    ~StatusGrpcClient();
    message::GetChatServerRsp GetChatServer(int uid);
    message::LoginRsp Login(int uid, std::string token);

private:
    StatusGrpcClient();

private:
    std::unique_ptr<StatusConPool> m_pool;
};