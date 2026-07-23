cmake_minimum_required(VERSION 3.16)
project(EveryOne LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_TOOLCHAIN_FILE /home/dingjr/software/vcpkg/scripts/buildsystems/vcpkg.cmake)

# 设置输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib)

find_package(protobuf CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)
find_package(jsoncpp REQUIRED)
find_package(hiredis REQUIRED)
find_package(Boost REQUIRED COMPONENTS system filesystem)

# find_package(PkgConfig REQUIRED)
find_path(MYSQLCPP_CONN_INCLUDE_DIR
    NAMES cppconn/connection.h
    PATHS /usr/include /usr/local/include
)

find_library(MYSQLCPP_CONN_LIBRARY
    NAMES mysqlcppconn
    PATHS /usr/lib /usr/lib/x86_64-linux-gnu /usr/local/lib
)

if(NOT MYSQLCPP_CONN_INCLUDE_DIR OR NOT MYSQLCPP_CONN_LIBRARY)
    message(FATAL_ERROR "Could not find system MySQL Connector/C++ headers or library")
endif()
    
set(CORE_SOURCES
    EveryOne/cserver.cc
    EveryOne/httpconnection.cc
    EveryOne/logicsystem.cc
    EveryOne/config_manager.cc
    EveryOne/message.pb.cc
    EveryOne/message.grpc.pb.cc
    EveryOne/iocontext_pool.cc
    EveryOne/verifygrpcclient.cc
    EveryOne/redismgr.cc
    EveryOne/mysql.cc
    EveryOne/mysql_dao.cc
    EveryOne/mysql_mgr.cc
    EveryOne/status_grpc_client.cc
)

add_library(EveryOnelib SHARED ${CORE_SOURCES})

target_include_directories(EveryOnelib PUBLIC
    ${MYSQLCPP_CONN_INCLUDE_DIR}
)
    
target_link_libraries(EveryOnelib PUBLIC
    protobuf::libprotobuf
    gRPC::grpc++
    jsoncpp
    Boost::system
    Boost::filesystem
    hiredis::hiredis
    pthread
    ${MYSQLCPP_CONN_LIBRARY}
)

# 2. 创建主程序可执行文件
add_executable(EveryOne
    EveryOne.cc
)

target_link_libraries(EveryOne PUBLIC
    EveryOnelib
)

# 2. 创建测试程序可执行文件
add_executable(testRedis
    test/testRedis.cc
)

target_link_libraries(testRedis PUBLIC
    EveryOnelib
)