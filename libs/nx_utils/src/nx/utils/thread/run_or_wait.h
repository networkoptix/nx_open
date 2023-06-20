// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>
#include <future>

#include <QtCore/QHash>

#include "mutex.h"

namespace nx {

/**
 * Wrapper class for long lasting functions. Calling this wrapper runs the long lasting function or
 * waits for the end of the in-progress function started with the same parameter.
 */
template<typename Parameter, typename Result>
class RunOrWait
{
public:
    RunOrWait(std::function<Result(const Parameter&)> func): m_function(std::move(func)) {}
    ~RunOrWait()
    {
        std::vector<std::shared_future<Result>> futures;
        {
            NX_MUTEX_LOCKER lock(&m_mutex);
            futures.reserve(m_started.size());
            for (auto& f: m_started)
                futures.emplace_back(std::move(f));
            m_started.clear();
        }
        for (auto& f: futures)
            f.wait();
    }

    Result operator()(const Parameter& parameter)
    {
        std::shared_future<Result> future;
        std::promise<Result> promise;
        {
            NX_MUTEX_LOCKER lock(&m_mutex);
            if (auto it = m_started.find(parameter); it != m_started.end())
            {
                future = it.value();
                lock.unlock();
                return future.get();
            }

            future = promise.get_future();
            m_started.emplace(parameter, future);
        }

        auto value = m_function(parameter);

        {
            NX_MUTEX_LOCKER lock(&m_mutex);
            m_started.remove(parameter);
        }

        promise.set_value(std::move(value));
        return future.get();
    }

private:
    std::function<Result(const Parameter&)> m_function;
    nx::Mutex m_mutex;
    QHash<Parameter, std::shared_future<Result>> m_started;
};

} // namespace nx
