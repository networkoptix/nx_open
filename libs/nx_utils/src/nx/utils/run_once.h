// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <future>
#include <map>
#include <memory>
#include <string_view>

#include <nx/utils/log/log.h>
#include <nx/utils/thread/mutex.h>

namespace nx::utils {

// The future returned by RunOnce.
using RunOnceFuture = std::future<void>;

namespace detail {

enum class TaskState
{
    idle,
    inProgress,
    reinit
};

// Helper for the RunOnce Keyless overload.
struct Keyless
{
};

// The states of a RunOnce that has a key: one per key, an absent key meaning TaskState::idle
template<typename Key>
class StateHolder
{
public:
    TaskState& at(const Key& key) { return m_states[key]; }
    void reset(const Key& key) { m_states.erase(key); }
    static nx::log::Tag logTag(const Key& key)
    {
        return nx::log::Tag(nx::format("RunOnce[%1]", key));
    }

private:
    std::map<Key, TaskState> m_states;
};

// The state of a RunOnce that has no key. Key is ignored.
template<>
class StateHolder<Keyless>
{
public:
    TaskState& at(Keyless) { return m_state; }
    void reset(Keyless) { m_state = TaskState::idle; }
    static nx::log::Tag logTag(Keyless)
    {
        return nx::log::Tag(QStringLiteral("RunOnce[Keyless]"));
    }

private:
    TaskState m_state = TaskState::idle;
};

} // namespace detail

// Runs a task asynchronously, at most one task at a time per key.
template<typename Key>
class RunOnce
{
public:
    // Schedule new task if it doesn't exists.
    std::optional<RunOnceFuture> startOnceAsync(std::function<void()> task, const Key& key)
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        return startOnceAsyncUnsafe(std::move(task), key);
    }

    // Schedule new task if it doesn't exists or it is running now. Multiple calls schedule only
    // one task but it is guarantee that new task is scheduled after `restartOnceAsync` call.
    std::optional<RunOnceFuture> restartOnceAsync(std::function<void()> task, const Key& key)
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        TaskState& state = m_stateHolder.at(key);
        if (state == TaskState::inProgress)
        {
            state = TaskState::reinit;
            return std::nullopt;
        }
        return startOnceAsyncUnsafe(std::move(task), key);
    }

private:
    using TaskState = detail::TaskState;
    using StateHolder = detail::StateHolder<Key>;

    nx::Mutex m_mutex;
    StateHolder m_stateHolder;

    // Starts the task if the key is idle: claims the state and creates the thread as one step.
    // The caller's lock must span both, or a state seen as inProgress while the thread may still
    // fail to exist lets a concurrent restartOnceAsync() set reinit that the rollback discards.
    std::optional<RunOnceFuture> startOnceAsyncUnsafe(std::function<void()> task, const Key& key)
    {
        TaskState& state = m_stateHolder.at(key);
        if (state != TaskState::idle)
            return std::nullopt;
        state = TaskState::inProgress;
        try
        {
            return std::async(std::launch::async,
                [this, task = std::move(task), key]() { runTaskLoop(task, key); });
        }
        catch (const std::exception& e)
        {
            // Thread creation may fail (EAGAIN) on low-memory devices. Roll the state back so that
            // a later call is able to retry, instead of letting the exception escape to the
            // caller, which used to terminate the process.
            NX_WARNING(StateHolder::logTag(key), "Failed to start an async task: %1", e.what());
            m_stateHolder.reset(key);
            return std::nullopt;
        }
    }

    void runTaskLoop(const std::function<void()>& task, const Key& key)
    {
        while (true)
        {
            try
            {
                task();
            }
            catch (const std::exception& e)
            {
                // A throwing task must not leave the state claimed, which would stall every
                // later call for this key. The exception itself is not swallowed: it still
                // reaches the caller through the returned future.
                resetStateAfterTaskFailure(e.what(), key);
                throw;
            }
            catch (...)
            {
                resetStateAfterTaskFailure("an unknown exception", key);
                throw;
            }

            NX_MUTEX_LOCKER lock(&m_mutex);
            TaskState& state = m_stateHolder.at(key);
            if (state == TaskState::inProgress)
            {
                m_stateHolder.reset(key);
                return;
            }
            state = TaskState::inProgress;
        }
    }

    void resetStateAfterTaskFailure(std::string_view reason, const Key& key)
    {
        NX_WARNING(StateHolder::logTag(key), "The async task has thrown %1", reason);
        NX_MUTEX_LOCKER lock(&m_mutex);
        m_stateHolder.reset(key);
    }
};

// Runs a task asynchronously, at most one task at a time.
template<>
class RunOnce<void>: public RunOnce<detail::Keyless>
{
    using Keyed = RunOnce<detail::Keyless>;

public:
    std::optional<RunOnceFuture> startOnceAsync(std::function<void()> task)
    {
        return Keyed::startOnceAsync(std::move(task), {});
    }

    std::optional<RunOnceFuture> restartOnceAsync(std::function<void()> task)
    {
        return Keyed::restartOnceAsync(std::move(task), {});
    }
};

} // namespace nx::utils
