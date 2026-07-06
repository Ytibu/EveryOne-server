#pragma once

#include <string>

class RedisMgr
{
public:
    RedisMgr();
    bool connect(const std::string &host, int port);

private:
};