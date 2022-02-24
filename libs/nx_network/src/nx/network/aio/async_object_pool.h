// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>
#include <unordered_map>

#include <nx/utils/move_only_func.h>
#include <nx/utils/thread/thread.h>

#include "aio_service.h"
#include "basic_pollable.h"

namespace nx::network::aio {

/**
 * Helps to scale some processing over the AIO thread pool by delegating work
 * in particular thread to a dedicated worker which operates in that thread only.
 *
 * Note: it is limited to AIO threads only to allow non-locking implementation:
 * thread-to-worker dictionary is pre-allocated in the constructor.
 * Relies on the AIO thread pool to stay unchanged during the whole life time.
 */
template<typename Worker>
requires std::is_base_of_v<BasicPollable, Worker>
class AsyncObjectPool
{
public:
    using FactoryFunc = nx::utils::MoveOnlyFunc<std::unique_ptr<Worker>()>;

    /**
     * thread-to-worker dictionary is pre-allocated here.
     * @param aioService Pool of AIO threads.
     * @param func Invoked to create a worker for a thread. Must be reenterable.
     */
    AsyncObjectPool(AIOService* aioService, FactoryFunc func):
        m_factoryFunc(std::move(func))
    {
        for (auto t: aioService->getAllAioThreads())
            m_workers[t->systemThreadId()] = nullptr;
    }

    ~AsyncObjectPool()
    {
        clear();
    }

    /**
     * @return An object instance that corresponds to the current thread. If there is no object
     * it is created by invoking factory func provided through the constructor.
     * If it is called not from an AIO thread that belongs to supplied AIOService, nullptr is returned.
     */
    Worker* get()
    {
        auto it = m_workers.find(nx::utils::Thread::currentThreadSystemId());
        if (it == m_workers.end())
            return nullptr;

        if (!it->second)
            it->second = m_factoryFunc();

        return it->second.get();
    }

    /**
     * Destroys every object invoking Worker::pleaseStopSync first.
     * So, it blocks and must not be invoked within AIO thread.
     * The caller needs to ensure that no AsyncObjectPool::get() is invoked concurrently with
     * AsyncObjectPool::clear(). Otherwise, it will be an undefined behavior.
     */
    void clear()
    {
        for (auto& [tid, worker]: m_workers)
        {
            if (!worker)
                continue;
            worker->pleaseStopSync();
            worker.reset();
        }
    }

private:
    FactoryFunc m_factoryFunc;
    std::unordered_map<nx::utils::ThreadId, std::unique_ptr<Worker>> m_workers;
};

} // namespace nx::network::aio
