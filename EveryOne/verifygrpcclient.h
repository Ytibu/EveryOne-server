#pragma once
#include "message.pb.h"
#include "message.grpc.pb.h"
#include "singleton.h"
#include "config_manager.h"
#include <grpcpp/grpcpp.h>
#include "header.h"

#include <chrono>
#include <condition_variable>
#include <memory>
#include <queue>
#include <mutex>

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;

using message::GetVerifyReq;
using message::GetVerifyRsp;
using message::VerifyService;

/**
 * @brief gRPC连接池类，用于管理与gRPC服务器的连接
 *  1. 顺序队列实现，先进先出
 *  2. 线程安全，支持多线程访问
 *  3. 支持连接池的关闭，通知所有等待的线程
 *  4. 支持连接池的清空，释放所有连接
 */
class RPCConPool
{
public:
    /**
     * @brief 创建并初始化gRPC连接池
     * 
     * @param pool_size 
     * @param host 
     * @param port 
     */
    RPCConPool(std::size_t pool_size, std::string host, std::string port);
    ~RPCConPool();
    void close();

    /**
     * @brief 从连接池中获取第一个gRPC连接
     * 
     * @return std::unique_ptr<VerifyService::Stub> 
     */
    std::unique_ptr<VerifyService::Stub> getConnection();

    /**
     * @brief 将gRPC连接释放回连接池
     * 
     * @param stub 
     */
    void releaseConnection(std::unique_ptr<VerifyService::Stub> stub);

private:
    std::atomic<bool> m_isStop;
    std::size_t m_poolSize;
    std::string m_host;
    std::string m_port;
    std::queue<std::unique_ptr<VerifyService::Stub>> m_connections;    // 存储gRPC连接的连接池
    std::condition_variable m_conVar;
    std::mutex m_mutex;
};

/**
 * @brief gRPC客户端类，用于与gRPC服务器进行通信
 * 
 */
class VerifyGrpcClient : public Singleton<VerifyGrpcClient>
{
    friend class Singleton<VerifyGrpcClient>;

public:
    /**
     * @brief 获取验证码
     * 
     * @param email 
     * @return GetVerifyRsp 
     */
    GetVerifyRsp GetVerifyCode(std::string email);

private:
    /**
     * @brief 构造函数，初始化gRPC连接池
     * 初始化配置实例，获取gRPC服务器的host和port，创建gRPC连接池
     */
    VerifyGrpcClient();

private:
    std::unique_ptr<RPCConPool> m_rpcPool;
};
