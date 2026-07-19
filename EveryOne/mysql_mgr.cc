#include "mysql_mgr.h"

MysqlMgr::MysqlMgr()
{

}

MysqlMgr::~MysqlMgr()
{

}

int MysqlMgr::registerUser(const std::string &username, const std::string &email, const std::string &password)
{
    return m_dao.registerUser(username, email, password);
}
bool MysqlMgr::checkEmail(const std::string &name, const std::string &email)
{
    return m_dao.checkEmail(name, email);
}
bool MysqlMgr::updatePassword(const std::string &name, const std::string &newPassword)
{
    return m_dao.updatePassword(name, newPassword);
}
bool MysqlMgr::checkPassword(const std::string &name, const std::string &password, UserInfo &userInfo)
{
    return m_dao.checkPassword(name, password, userInfo);
}