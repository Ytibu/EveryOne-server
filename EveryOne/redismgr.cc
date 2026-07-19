#include "redismgr.h"
#include "config_manager.h"
#include "logger.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>

// ============================================================================
// RedisConPool — 连接池实现
// ============================================================================

RedisConPool::RedisConPool(std::size_t pool_size, const std::string &host, int port, const std::string &password)
    : m_isStop(false), m_poolSize(pool_size), m_host(host), m_port(port), m_password(password)
{
    for (std::size_t i = 0; i < m_poolSize; ++i)
    {
        redisContext *ctx = createConnection();
        if (ctx != nullptr)
        {
            m_connections.push(ctx);
        }
    }

    LOG_INFO(std::string("RedisConPool initialized: ") + m_host + ":"
             + std::to_string(m_port) + " pool_size=" + std::to_string(m_poolSize)
             + " actual=" + std::to_string(m_connections.size()));
}

RedisConPool::~RedisConPool()
{
    close();
}

redisContext *RedisConPool::createConnection()
{
    // 同步连接 Redis
    redisContext *ctx = redisConnect(m_host.c_str(), m_port);
    if (ctx == nullptr || ctx->err)
    {
        if (ctx)
        {
            LOG_ERROR(std::string("RedisConPool::createConnection failed: ") + ctx->errstr);
            redisFree(ctx);
        }
        else
        {
            LOG_ERROR("RedisConPool::createConnection failed: can't allocate redis context");
        }
        return nullptr;
    }

    // 如果配置了密码，发送 AUTH 命令
    if (!m_password.empty())
    {
        redisReply *reply = (redisReply *)redisCommand(ctx, "AUTH %s", m_password.c_str());
        if (reply == nullptr || reply->type == REDIS_REPLY_ERROR)
        {
            LOG_ERROR(std::string("RedisConPool::AUTH failed: ")
                      + (reply ? reply->str : ctx->errstr));
            freeReplyObject(reply);
            redisFree(ctx);
            return nullptr;
        }
        freeReplyObject(reply);
    }

    return ctx;
}

void RedisConPool::close()
{
    m_isStop = true;
    m_conVar.notify_all();

    std::lock_guard<std::mutex> lock(m_mutex);
    while (!m_connections.empty())
    {
        redisContext *ctx = m_connections.front();
        m_connections.pop();
        if (ctx)
        {
            redisFree(ctx);
        }
    }
}

redisContext *RedisConPool::getConnection()
{
    std::unique_lock<std::mutex> lock(m_mutex);
    m_conVar.wait(lock, [this] {
        return !m_connections.empty() || m_isStop;
    });

    if (m_isStop)
    {
        return nullptr;
    }

    redisContext *ctx = m_connections.front();
    m_connections.pop();
    return ctx;
}

void RedisConPool::releaseConnection(redisContext *ctx)
{
    if (ctx == nullptr)
    {
        return;
    }

    // 检查连接是否仍然健康，如果异常则丢弃该连接
    if (ctx->err)
    {
        LOG_WARN(std::string("RedisConPool discarding broken connection: ") + ctx->errstr);
        redisFree(ctx);
        // 尝试补充一个新连接
        redisContext *newCtx = createConnection();
        if (newCtx)
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (!m_isStop)
            {
                m_connections.push(newCtx);
                m_conVar.notify_one();
            }
            else
            {
                redisFree(newCtx);
            }
        }
        return;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_isStop)
    {
        redisFree(ctx);
        return;
    }

    m_connections.push(ctx);
    m_conVar.notify_one();
}

// ============================================================================
// RedisMgr — Redis 管理器实现
// ============================================================================

RedisMgr::RedisMgr()
    : m_isStop(false)
{
    auto &config = ConfigManager::GetInstance();
    std::string host = config["redis"]["host"];
    std::string port_str = config["redis"]["port"];
    std::string pass = config["redis"]["password"];
    std::string pool_size_str = config["redis"]["pool_size"];

    if (host.empty())
        host = "127.0.0.1";
    int port = port_str.empty() ? 6379 : std::stoi(port_str);
    std::size_t pool_size = pool_size_str.empty() ? 5 : std::stoul(pool_size_str);

    LOG_INFO(std::string("RedisMgr connecting to: ") + host + ":"
             + std::to_string(port) + " pool_size=" + std::to_string(pool_size));

    m_pool.reset(new RedisConPool(pool_size, host, port, pass));
}

RedisMgr::~RedisMgr()
{
    m_isStop = true;
    if (m_pool)
    {
        m_pool->close();
    }
}

redisReply *RedisMgr::execCommand(const std::string &cmd)
{
    if (m_isStop || !m_pool)
    {
        return nullptr;
    }

    redisContext *ctx = m_pool->getConnection();
    if (ctx == nullptr)
    {
        return nullptr;
    }

    redisReply *reply = (redisReply *)redisCommand(ctx, cmd.c_str());
    m_pool->releaseConnection(ctx);
    return reply;
}

redisReply *RedisMgr::ExecuteCmd(const char *fmt, ...)
{
    if (m_isStop || !m_pool)
    {
        return nullptr;
    }

    redisContext *ctx = m_pool->getConnection();
    if (ctx == nullptr)
    {
        return nullptr;
    }

    va_list ap;
    va_start(ap, fmt);
    redisReply *reply = (redisReply *)redisvCommand(ctx, fmt, ap);
    va_end(ap);

    m_pool->releaseConnection(ctx);
    return reply;
}

// ==================== String 操作 ====================

bool RedisMgr::Get(const std::string &key, std::string &value)
{
    redisReply *reply = execCommand("GET " + key);
    if (reply == nullptr)
    {
        return false;
    }

    bool ok = false;
    if (reply->type == REDIS_REPLY_STRING)
    {
        value.assign(reply->str, reply->len);
        ok = true;
    }
    freeReplyObject(reply);
    return ok;
}

bool RedisMgr::Set(const std::string &key, const std::string &value)
{
    // 使用引号确保特殊字符安全
    redisReply *reply = execCommand("SET " + key + " " + value);
    if (reply == nullptr)
    {
        return false;
    }

    bool ok = (reply->type == REDIS_REPLY_STATUS
               && strcasecmp(reply->str, "OK") == 0);
    freeReplyObject(reply);
    return ok;
}

bool RedisMgr::SetWithExpire(const std::string &key, const std::string &value, int ttl)
{
    redisReply *reply = execCommand("SET " + key + " " + value
                                    + " EX " + std::to_string(ttl));
    if (reply == nullptr)
    {
        return false;
    }

    bool ok = (reply->type == REDIS_REPLY_STATUS
               && strcasecmp(reply->str, "OK") == 0);
    freeReplyObject(reply);
    return ok;
}

// ==================== Key 操作 ====================

bool RedisMgr::ExistsKey(const std::string &key)
{
    redisReply *reply = execCommand("EXISTS " + key);
    if (reply == nullptr)
    {
        return false;
    }

    bool ok = (reply->type == REDIS_REPLY_INTEGER && reply->integer > 0);
    freeReplyObject(reply);
    return ok;
}

bool RedisMgr::Del(const std::string &key)
{
    redisReply *reply = execCommand("DEL " + key);
    if (reply == nullptr)
    {
        return false;
    }

    bool ok = (reply->type == REDIS_REPLY_INTEGER && reply->integer > 0);
    freeReplyObject(reply);
    return ok;
}

bool RedisMgr::Expire(const std::string &key, int ttl)
{
    redisReply *reply = execCommand("EXPIRE " + key + " " + std::to_string(ttl));
    if (reply == nullptr)
    {
        return false;
    }

    bool ok = (reply->type == REDIS_REPLY_INTEGER && reply->integer > 0);
    freeReplyObject(reply);
    return ok;
}

// ==================== List 操作 ====================

bool RedisMgr::LPush(const std::string &key, const std::string &value)
{
    redisReply *reply = execCommand("LPUSH " + key + " " + value);
    if (reply == nullptr)
    {
        return false;
    }

    bool ok = (reply->type == REDIS_REPLY_INTEGER);
    freeReplyObject(reply);
    return ok;
}

bool RedisMgr::LPop(const std::string &key, std::string &value)
{
    redisReply *reply = execCommand("LPOP " + key);
    if (reply == nullptr)
    {
        return false;
    }

    bool ok = false;
    if (reply->type == REDIS_REPLY_STRING)
    {
        value.assign(reply->str, reply->len);
        ok = true;
    }
    freeReplyObject(reply);
    return ok;
}

bool RedisMgr::BRPop(const std::string &key, std::string &value, int timeout)
{
    redisReply *reply = execCommand("BRPOP " + key + " "
                                    + std::to_string(timeout));
    if (reply == nullptr)
    {
        return false;
    }

    bool ok = false;
    // BRPOP 返回包含 [key, value] 的数组，或者 nil（超时）
    if (reply->type == REDIS_REPLY_ARRAY && reply->elements >= 2)
    {
        redisReply *valElem = reply->element[1];
        if (valElem->type == REDIS_REPLY_STRING)
        {
            value.assign(valElem->str, valElem->len);
            ok = true;
        }
    }
    freeReplyObject(reply);
    return ok;
}

bool RedisMgr::LLen(const std::string &key, int &len)
{
    redisReply *reply = execCommand("LLEN " + key);
    if (reply == nullptr)
    {
        return false;
    }

    bool ok = false;
    if (reply->type == REDIS_REPLY_INTEGER)
    {
        len = static_cast<int>(reply->integer);
        ok = true;
    }
    freeReplyObject(reply);
    return ok;
}

// ==================== Hash 操作 ====================

bool RedisMgr::HSet(const std::string &key, const std::string &field,
                    const std::string &value)
{
    redisReply *reply = execCommand("HSET " + key + " " + field + " " + value);
    if (reply == nullptr)
    {
        return false;
    }

    // HSET 返回 1（新增）或 0（覆盖）
    bool ok = (reply->type == REDIS_REPLY_INTEGER);
    freeReplyObject(reply);
    return ok;
}

bool RedisMgr::HGet(const std::string &key, const std::string &field,
                    std::string &value)
{
    redisReply *reply = execCommand("HGET " + key + " " + field);
    if (reply == nullptr)
    {
        return false;
    }

    bool ok = false;
    if (reply->type == REDIS_REPLY_STRING)
    {
        value.assign(reply->str, reply->len);
        ok = true;
    }
    freeReplyObject(reply);
    return ok;
}

bool RedisMgr::HExists(const std::string &key, const std::string &field)
{
    redisReply *reply = execCommand("HEXISTS " + key + " " + field);
    if (reply == nullptr)
    {
        return false;
    }

    bool ok = (reply->type == REDIS_REPLY_INTEGER && reply->integer > 0);
    freeReplyObject(reply);
    return ok;
}

bool RedisMgr::HDel(const std::string &key, const std::string &field)
{
    redisReply *reply = execCommand("HDEL " + key + " " + field);
    if (reply == nullptr)
    {
        return false;
    }

    bool ok = (reply->type == REDIS_REPLY_INTEGER && reply->integer > 0);
    freeReplyObject(reply);
    return ok;
}
