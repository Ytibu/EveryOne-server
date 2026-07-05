#pragma once
#include "message.pb.h"
#include "message.grpc.pb.h"
#include "singleton.h"
#include "config_manager.h"
#include <grpcpp/grpcpp.h>
#include "header.h"

#include <chrono>

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;

using message::GetVerifyReq;
using message::GetVerifyRsp;
using message::VerifyService;

class RPCConPool
{
public:
    RPCConPool(std::size_t pool_size, std::string host, std::string port);
    ~RPCConPool();
    void close();
    std::unique_ptr<VerifyService::Stub> getConnection();
    void releaseConnection(std::unique_ptr<VerifyService::Stub> stub);

private:
    std::atomic<bool> m_isStop;
    std::size_t m_poolSize;
    std::string m_host;
    std::string m_port;
    std::vector<std::unique_ptr<VerifyService::Stub>> m_connections;
    std::condition_variable m_conVar;
    std::mutex m_mutex;
};

class VerifyGrpcClient : public Singleton<VerifyGrpcClient>
{
    friend class Singleton<VerifyGrpcClient>;

public:
    GetVerifyRsp GetVerifyCode(std::string email);

private:
    VerifyGrpcClient();

private:
    std::unique_ptr<VerifyService::Stub> stub_;
    std::unique_ptr<RPCConPool> m_rpcPool;
};
