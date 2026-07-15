#pragma once

#include <string>
#include <memory>
#include "mysql.h"
#include "singleton.h"

class MysqlDao
{
public:
    MysqlDao();
    ~MysqlDao();
    bool registerUser(const std::string &username, const std::string &email, const std::string &password);
    bool checkEmail(const std::string &name, const std::string &email);
    bool updatePassword(const std::string &name, const std::string &newPassword);
    bool checkPassword(const std::string &name, const std::string &password, UserInfo &userInfo);

private:
    std::unique_ptr<MySqlPool> m_pool;
};