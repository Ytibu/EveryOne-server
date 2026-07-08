#include "../EveryOne/redismgr.h"

void testRedis() 
{

    RedisConPool pool(5, "localhost", 6379, "123450"); // 创建一个连接池，连接数为5
}

int main(int argc, char const *argv[])
{
    testRedis();

    return 0;
}
