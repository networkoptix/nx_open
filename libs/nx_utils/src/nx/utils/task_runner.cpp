// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "task_runner.h"

#include <algorithm>
#include <cassert>
#include <thread>

// System network headers must be included before boost::asio to ensure no-pch build on Windows.
#include <nx/utils/system_network_headers.h>

#include <boost/asio/io_service.hpp>
#include <boost/bind.hpp>

#include <nx/utils/log/log.h>

namespace nx::utils {

struct TaskRunner::Impl
{
    boost::asio::io_service ioService;
    std::deque<std::thread> threadPool;
    std::unique_ptr<boost::asio::io_service::work> ioServiceWork;
};

//-------------------------------------------------------------------------------------------------
// TaskRunner implementation.

TaskRunner::TaskRunner(int threadsNumber):
    m_threadsNumber(threadsNumber),
    m_impl(new Impl)
{
}

TaskRunner::~TaskRunner() noexcept
{
}

void TaskRunner::activateObjectHook()
{
    NX_ASSERT(m_impl->threadPool.empty());

    // Make sure that ioService run() will not return.
    m_impl->ioServiceWork.reset(new boost::asio::io_service::work(m_impl->ioService));

    // Start running of tasks.
    for(int threadIndex = 0; threadIndex < m_threadsNumber; ++threadIndex)
    {
        m_impl->threadPool.emplace_back(std::thread(
            boost::bind(&boost::asio::io_service::run, &m_impl->ioService)));
    }
}

void TaskRunner::deactivateObjectHook()
{
    // Signal to stop tasks running.
    m_impl->ioService.stop();
}

void TaskRunner::waitObjectHook()
{
    for (auto& thread : m_impl->threadPool)
    {
        thread.join();
    }

    m_impl->threadPool.clear();
}

void TaskRunner::clear()
{
    const std::lock_guard<std::mutex> guard(m_lock);
    m_impl->ioServiceWork.reset();
}

void TaskRunner::runTask(Task task)
{
    std::shared_ptr<Task> copyTask(new Task(std::move(task)));
    m_impl->ioService.post([ copyTask ] { (*copyTask)(); });
}

} // namespace nx::utils
