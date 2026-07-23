#include "iocontext_pool.h"
#include "logger.h"

IOContextPool::IOContextPool(std::size_t pool_size)
    : m_ioService(pool_size), m_works(pool_size), m_nextIOService(0)
{
    if (pool_size == 0) throw std::runtime_error("IOContextPool size is 0");

    for (std::size_t i = 0; i < m_ioService.size(); ++i)
    {
        m_works[i] = std::make_unique<Work>( boost::asio::make_work_guard(m_ioService[i]) );
    }

    // 为每个 io_context 创建一个驱动线程
    for (std::size_t i = 0; i < m_ioService.size(); ++i)
    {
        m_threads.emplace_back([this, i]() {
            try
            {
                m_ioService[i].run();
            }
            catch (const std::exception &e)
            {
                // 业务 handler 异常穿透至此；线程退出但池整体不受影响
                LOG_ERROR(std::string("IOContextPool thread exception: ") + e.what());
            }
        });
    }
}

IOContextPool::~IOContextPool()
{
    Stop();
}

boost::asio::io_context &IOContextPool::GetIOService()
{
    return m_ioService[ m_nextIOService.fetch_add(1, std::memory_order_relaxed) % m_ioService.size()];
}

void IOContextPool::Stop()
{
    // 先停止所有 io_context，取消未完成的异步操作
    for (auto &ctx : m_ioService)
    {
        ctx.stop();
    }

    // 再销毁 work_guard，此时 io_context 已停止，不会重新唤醒
    for (auto &work : m_works)
    {
        work.reset();
    }

    // 等待所有驱动线程退出
    for (auto &thread : m_threads)
    {
        thread.join();
    }
}