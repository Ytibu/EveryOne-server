#pragma once

#include <string>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <memory>
#include <functional>
#include <hiredis/hiredis.h>

#include "singleton.h"

/**
 * @brief Redis 连接池，管理与 Redis 服务器的连接
 *
 * 采用队列实现 FIFO 策略，线程安全，支持优雅关闭。
 * 与 RPCConPool 保持一致的连接池设计风格。
 */
class RedisConPool
{
public:
    /**
     * @brief 创建并初始化 Redis 连接池
     * @param pool_size  连接数量
     * @param host       Redis 主机地址
     * @param port       Redis 端口
     * @param password   Redis 密码（可选）
     */
    RedisConPool(std::size_t pool_size, const std::string &host, int port,
                 const std::string &password = "");
    ~RedisConPool();

    /** 关闭连接池，通知所有等待的线程并释放所有连接 */
    void close();

    /** 从连接池获取一个 Redis 连接 */
    redisContext *getConnection();

    /** 将 Redis 连接归还连接池 */
    void releaseConnection(redisContext *ctx);

private:
    /** 创建一个新的 Redis 连接 */
    redisContext *createConnection();

    std::atomic<bool> m_isStop;
    std::size_t m_poolSize;
    std::string m_host;
    int m_port;
    std::string m_password;
    std::queue<redisContext *> m_connections;
    std::condition_variable m_conVar;
    std::mutex m_mutex;
};

/**
 * @brief Redis 管理器，单例模式，封装常用 Redis 操作
 *
 * 提供 Get、Set、Del、Exists 等常用命令，
 * 以及 List、Hash、Key 等类型的便捷接口。
 */
class RedisMgr : public Singleton<RedisMgr>
{
    friend class Singleton<RedisMgr>;

public:
    ~RedisMgr();

    // ==================== 基础命令 ====================

    /** 连接池是否已关闭 */
    bool isStop() const { return m_isStop; }

    // ==================== String 操作 ====================

    /**
     * @brief 获取指定 key 的值
     * @param key
     * @param value 输出参数
     * @return true 成功，false 失败或 key 不存在
     */
    bool Get(const std::string &key, std::string &value);

    /**
     * @brief 设置 key-value
     * @param key
     * @param value
     * @return true 成功
     */
    bool Set(const std::string &key, const std::string &value);

    /**
     * @brief 设置 key-value 并指定过期时间（秒）
     * @param key
     * @param value
     * @param ttl  过期秒数
     * @return true 成功
     */
    bool SetWithExpire(const std::string &key, const std::string &value, int ttl);

    // ==================== Key 操作 ====================

    /** 检查 key 是否存在 */
    bool ExistsKey(const std::string &key);

    /** 删除指定 key */
    bool Del(const std::string &key);

    /** 设置 key 的过期时间（秒） */
    bool Expire(const std::string &key, int ttl);

    // ==================== List 操作 ====================

    /** 从列表左侧插入元素 */
    bool LPush(const std::string &key, const std::string &value);

    /** 从列表左侧弹出元素 */
    bool LPop(const std::string &key, std::string &value);

    /** 从列表右侧弹出元素（阻塞，带超时） */
    bool BRPop(const std::string &key, std::string &value, int timeout = 0);

    /** 获取列表长度 */
    bool LLen(const std::string &key, int &len);

    // ==================== Hash 操作 ====================

    /** 设置哈希表字段 */
    bool HSet(const std::string &key, const std::string &field, const std::string &value);

    /** 获取哈希表字段 */
    bool HGet(const std::string &key, const std::string &field, std::string &value);

    /** 检查哈希表字段是否存在 */
    bool HExists(const std::string &key, const std::string &field);

    /** 删除哈希表字段 */
    bool HDel(const std::string &key, const std::string &field);

    // ==================== 通用执行接口 ====================

    /**
     * @brief 执行任意 Redis 命令（高级接口）
     * @param fmt  printf 风格格式字符串
     * @param ...  可变参数
     * @return redisReply*  调用者需调用 freeReplyObject() 释放
     */
    redisReply *ExecuteCmd(const char *fmt, ...);

private:
    RedisMgr();

    /** 执行一条命令，自动管理连接获取/归还 */
    redisReply *execCommand(const std::string &cmd);

private:
    bool m_isStop;
    std::unique_ptr<RedisConPool> m_pool;
};
