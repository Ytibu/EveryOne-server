#include "mysql_dao.h"

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
    return false;
}

bool MysqlDao::updatePassword(const std::string &name, const std::string &newPassword)
{
    return false;
}

bool MysqlDao::checkPassword(const std::string &name, const std::string &password, UserInfo &userInfo)
{
    return false;
}