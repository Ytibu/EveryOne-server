#pragma once

#include <string>
#include <memory>
#include "mysql.h"
#include "mysql_dao.h"
#include "singleton.h"

class MysqlMgr : public Singleton<MysqlMgr>
{
    friend class Singleton<MysqlMgr>;
public:
    ~MysqlMgr();
    int registerUser(const std::string &username, const std::string &email, const std::string &password);
    bool checkEmail(const std::string &name, const std::string &email);
    bool updatePassword(const std::string &name, const std::string &newPassword);
    bool checkPassword(const std::string &name, const std::string &password, UserInfo &userInfo);

private:
    MysqlMgr();

private:
    MysqlDao m_dao;
};