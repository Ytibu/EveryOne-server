#pragma once
#include <vector>
#include <memory>
#include <thread>
#include <atomic>

#include <boost/asio.hpp>
#include <boost/asio/io_context.hpp>

#include "singleton.h"

class IOContextPool : public Singleton<IOContextPool>
{
    friend class Singleton<IOContextPool>;

public:
    using IOService = boost::asio::io_context;
    using Work = boost::asio::executor_work_guard<boost::asio::io_context::executor_type>;
    using WorkPtr = std::unique_ptr<Work>;

public:
    IOContextPool(const IOContextPool &) = delete;
    IOContextPool &operator=(const IOContextPool &) = delete;
    ~IOContextPool();

    boost::asio::io_context &GetIOService();
    void Stop();

private:
    explicit IOContextPool(std::size_t pool_size = std::thread::hardware_concurrency());

private:
    std::vector<IOService> m_ioService;
    std::vector<std::thread> m_threads;
    std::vector<WorkPtr> m_works;
    std::atomic<std::size_t> m_nextIOService;
};