cmake_minimum_required(VERSION 3.16)
project(EveryOne LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_TOOLCHAIN_FILE /home/dingjr/software/vcpkg/scripts/buildsystems/vcpkg.cmake)

find_package(protobuf CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)
find_package(jsoncpp REQUIRED)
find_package(hiredis REQUIRED)
find_package(Boost REQUIRED COMPONENTS system filesystem)
    
add_executable(EveryOne
    EveryOne/cserver.cc
    EveryOne/httpconnection.cc
    EveryOne/logicsystem.cc
    EveryOne/config_manager.cc
    EveryOne/message.pb.cc
    EveryOne/message.grpc.pb.cc
    EveryOne/iocontext_pool.cc
    EveryOne/verifygrpcclient.cc
    EveryOne.cc
)
    
target_link_libraries(EveryOne PRIVATE
    protobuf::libprotobuf
    gRPC::grpc++
    jsoncpp
    Boost::system
    Boost::filesystem
    hiredis::hiredis
    pthread
)