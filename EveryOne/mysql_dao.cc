#include "mysql_dao.h"

#include "header.h"
#include "logger.h"
#include "config_manager.h"
#include <cppconn/exception.h>
#include <cppconn/driver.h>
#include <cppconn/statement.h>
#include <cppconn/prepared_statement.h>

MysqlDao::MysqlDao()
{
    auto config = ConfigManager::GetInstance();
    const auto &host = config["mysql"]["host"];
    const auto &port = config["mysql"]["port"];
    const auto &password = config["mysql"]["password"];
    const auto &database = config["mysql"]["database"];
    const auto &user = config["mysql"]["user"];
    m_pool.reset(new MySqlPool(host + ":" + port, user, password, database, 10));
}

MysqlDao::~MysqlDao()
{
    m_pool->close();
}

int MysqlDao::registerUser(const std::string &username, const std::string &email, const std::string &password)
{
    auto conn = m_pool->getConnection();
    try{
        if(conn == nullptr){
            return 1;
        }

        // 准备调用存储过程
        std::unique_ptr<sql::PreparedStatement> pstmt(conn->conn_->prepareStatement("CALL reg_user(?, ?, ?, @result)"));
        
        // 设置输入参数
        pstmt->setString(1, username);
        pstmt->setString(2, email);
        pstmt->setString(3, password);
        
        // 执行存储过程
        pstmt->executeUpdate();

        // 获取输出参数
        std::unique_ptr<sql::ResultSet> res(conn->conn_->createStatement()->executeQuery("SELECT @result AS result"));
        if (res->next()) {
            int result = res->getInt("result");
            LOG_INFO("registerUser result: " + std::to_string(result));
            m_pool->releaseConnection(std::move(conn));
            return result;
        }
        m_pool->releaseConnection(std::move(conn));
        return 0; // 如果没有结果，返回错误码

    }catch(const sql::SQLException &e){
        LOG_ERROR(std::string("MysqlDao registerUser failed: ") + e.what());
        m_pool->releaseConnection(std::move(conn));
        return 0; // 返回错误码
    }
}

bool MysqlDao::checkEmail(const std::string &name, const std::string &email)
{
    auto conn = m_pool->getConnection();
    try{
        if(conn == nullptr){
            return false;
        }

        // 准备调用存储过程
        std::unique_ptr<sql::PreparedStatement> pstmt(conn->conn_->prepareStatement("SELECT email FROM user WHERE name = ?"));
        
        // 设置输入参数
        pstmt->setString(1, name);
        
        // 执行查询
        std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());

        // 检查查询结果
        while(res->next()) {
            std::string result = res->getString("email");
            LOG_INFO("checkEmail result: " + result);

            if(result != email){
                m_pool->releaseConnection(std::move(conn));
                return false; // 邮箱不匹配
            }
            m_pool->releaseConnection(std::move(conn));
            return true; // 邮箱匹配
        }
        return true;

    }catch(const sql::SQLException &e){
        LOG_ERROR(std::string("MysqlDao checkEmail failed: ") + e.what());
        m_pool->releaseConnection(std::move(conn));
        return false;
    }
}

bool MysqlDao::updatePassword(const std::string &name, const std::string &newPassword)
{
    auto conn = m_pool->getConnection();
    try{
        if(conn == nullptr){
            return false;
        }

        // 准备调用存储过程
        std::unique_ptr<sql::PreparedStatement> pstmt(conn->conn_->prepareStatement("UPDATE user SET pwd = ? WHERE name = ?"));
        
        // 设置输入参数
        pstmt->setString(1, newPassword);
        pstmt->setString(2, name);
        
        // 执行存储过程
        int updateCount = pstmt->executeUpdate();
        LOG_INFO("updatePassword updateCount: " + std::to_string(updateCount));
        LOG_INFO("updatePassword name: " + name + ", newPassword: " + newPassword);
        m_pool->releaseConnection(std::move(conn));
        return true;
    }catch(const sql::SQLException &e){
        LOG_ERROR("SQLException: " + std::string(e.what()));
        m_pool->releaseConnection(std::move(conn));
        return false;
    }
}

bool MysqlDao::checkPassword(const std::string &email, const std::string &password, UserInfo &userInfo)
{
    LOG_INFO(email + " :: " + password);
    auto conn = m_pool->getConnection();
    if(conn == nullptr){
            return false;
        }

    Defer defer([this, &conn](){
        m_pool->releaseConnection(std::move(conn));
    });

    try{
        // mysql查询语句，使用预处理语句防止SQL注入
        std::unique_ptr<sql::PreparedStatement> pstmt(conn->conn_->prepareStatement("SELECT * FROM user WHERE email = ?"));
        
        // 设置输入参数
        pstmt->setString(1, email);
        
        // 执行查询
        std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());

        // 检查查询结果
        std::string originalPassword = "";
        while (res->next())
        {
            originalPassword = res->getString("pwd");
            LOG_INFO("checkPassword originalPassword: " + originalPassword);
            break;
        }

        if(originalPassword != password){
            LOG_WARN("checkPassword failed: password mismatch or user not found");
            return false; // 密码不匹配或用户不存在
        }

        userInfo.username = res->getString("name");
        userInfo.email = res->getString("email");
        userInfo.uid = res->getInt("uid");
        userInfo.password = originalPassword;

        m_pool->releaseConnection(std::move(conn));
        return true;

    }catch(const sql::SQLException &e){
        LOG_ERROR("SQLException: " + std::string(e.what()));
        m_pool->releaseConnection(std::move(conn));
        return false;
    }
}