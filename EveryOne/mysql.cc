#include "mysql.h"
#include "logger.h"
#include "config_manager.h"

#include <cppconn/exception.h>
#include <cppconn/driver.h>
#include <chrono>
#include <cppconn/statement.h>
#include <cppconn/prepared_statement.h>

MySqlPool::MySqlPool(const std::string &url, const std::string &user,
                     const std::string &password, const std::string &database, int pool_size)
    : m_url(url), m_user(user), m_password(password), m_database(database), m_pool_size(pool_size), m_stop(false)
{
    try
    {
        for (int i = 0; i < m_pool_size; ++i)
        {
            sql::Driver *driver = get_driver_instance();
            sql::Connection *conn = driver->connect(m_url, m_user, m_password);
            conn->setSchema(m_database);

            auto now = std::chrono::system_clock::now().time_since_epoch();

            m_connPool.push(std::make_unique<SqlConnection>(conn, std::chrono::duration_cast<std::chrono::seconds>(now).count()));
        }

        m_check_thread = std::thread([this](){
            while (!m_stop)
            {
                checkConnection();
                std::this_thread::sleep_for(std::chrono::seconds(60));
            }
            
        });

        m_check_thread.detach();
    }
    catch (const std::exception &e)
    {
        LOG_ERROR(std::string("MySqlPool initialization failed: ") + e.what());
    }
}

void MySqlPool::checkConnection()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    int pool_size = m_connPool.size();
    auto now = std::chrono::system_clock::now().time_since_epoch();
    long long current_time = std::chrono::duration_cast<std::chrono::seconds>(now).count();
    for (int i = 0; i < pool_size; i++)
    {
        auto conn = std::move(m_connPool.front());
        m_connPool.pop();

        Defer defer([this, &conn]() {
            m_connPool.push(std::move(conn));
        });

        if(current_time - conn->m_last_active_time < 5){
            continue;
        }

        try{
            std::unique_ptr<sql::Statement> stmt(conn->conn_->createStatement());
            stmt->executeQuery("SELECT 1");
            conn->m_last_active_time = current_time;
        }catch(const sql::SQLException &e){
            LOG_ERROR(std::string("MySqlPool checkConnection failed: ") + e.what());

            // 创建一个新的连接替换掉失效的连接
            sql::Driver *driver = get_driver_instance();
            sql::Connection *new_conn = driver->connect(m_url, m_user, m_password);
            new_conn->setSchema(m_database);
            conn->conn_.reset(new_conn);
            conn->m_last_active_time = current_time;
        }
    }
    
}

MySqlPool::~MySqlPool()
{
    std::unique_lock<std::mutex> lock(m_mutex);
    while (!m_connPool.empty())
    {
        m_connPool.pop();
    }
}

std::unique_ptr<SqlConnection> MySqlPool::getConnection()
{
    std::unique_lock<std::mutex> lock(m_mutex);
    m_condition.wait(lock, [this]() {
        if(m_stop) {
            LOG_ERROR("MySqlPool is stopping, cannot get connection.");
            return true;
        }
        return !m_connPool.empty(); 
    });

    if(m_stop) {
        return nullptr;
    }

    std::unique_ptr<SqlConnection> conn(std::move(m_connPool.front()));
    m_connPool.pop();
    return conn;
}

void MySqlPool::releaseConnection(std::unique_ptr<SqlConnection> conn)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if(m_stop)
    {
        return;
    }
    
    m_connPool.push(std::move(conn));
    m_condition.notify_one();
}

void MySqlPool::close()
{
    m_stop = true;
    m_condition.notify_all();
}