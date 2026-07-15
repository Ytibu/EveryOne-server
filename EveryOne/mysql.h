#pragma once
#include <cppconn/connection.h>
#include <memory>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <functional>

/**
 * @brief 一个简单的RAII（资源获取与释放）风格类，用于在函数作用域结束时执行特定函数。
 * 
 */
class Defer
{
public:
    Defer(std::function<void()> func) : m_func(func) {}
    ~Defer() { m_func(); }

private:
    std::function<void()> m_func;
};

class SqlConnection
{
public:
    SqlConnection(sql::Connection *conn, int64_t last_active_time)
        : conn_(conn), m_last_active_time(last_active_time) {}
    std::unique_ptr<sql::Connection> conn_;
    int64_t m_last_active_time;
};

class MySqlPool
{
public:
    MySqlPool(const std::string &url, const std::string &user, const std::string &password, const std::string &database, int pool_size);
    ~MySqlPool();

    std::unique_ptr<SqlConnection> getConnection();
    void releaseConnection(std::unique_ptr<SqlConnection> conn);
    void close();
    void checkConnection();

private:
    std::string m_url;
    std::string m_user;
    std::string m_password;
    std::string m_database;
    int m_pool_size;
    std::queue<std::unique_ptr<SqlConnection>> m_connPool;
    std::mutex m_mutex;
    std::condition_variable m_condition;
    std::atomic<bool> m_stop;
    std::thread m_check_thread;
};

struct UserInfo
{
    std::string username;
    std::string password;
    int uid;
    std::string email;
};