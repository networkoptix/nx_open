// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <future>
#include <memory>

#include <nx/utils/lockable.h>

namespace nx::utils {

class RunOnceBase
{
public:
    using Future = std::future<void>;

protected:
    enum class State
    {
        idle,
        inProgress,
        reinit
    };
};

template<typename T>
class RunOnce: public RunOnceBase
{
public:
    RunOnce() = default;

    /*
     * Schedule new task if it doesn't exists.
     */
    std::optional<Future> startOnceAsync(std::function<void()> task, const T& key)
    {
        if (switchState(key, State::idle, State::inProgress))
            return startTaskInternal(std::move(task), key);
        return std::nullopt;
    }

    /*
     * Schedule new task if it doesn't exists or it is running now.
     * Multiple calls schedule only one task but it is guarantee that
     * new task is scheduled after `restartOnceAsync` call.
     */
    std::optional<Future> restartOnceAsync(std::function<void()> task, const T& key)
    {
        if (switchState(key, State::inProgress, State::reinit))
            return std::nullopt;
        return startOnceAsync(std::move(task), key);
    }

private:
    nx::Lockable<std::map<T, State>> m_state;

    bool switchState(const T& key, State from, State to)
    {
        auto state = m_state.lock();
        auto& value = (*state)[key];
        if (value != from)
            return false;
        value = to;
        return true;
    }

    Future startTaskInternal(std::function<void()> task, const T& key)
    {
        return std::async(std::launch::async,
            [this, task = std::move(task), key]()
            {
                while (true)
                {
                    task();
                    auto state = m_state.lock();
                    auto& value = (*state)[key];
                    if (value == State::inProgress)
                    {
                        state->erase(key);
                        break;
                    }
                    value = State::inProgress;
                }
            });
    }
};

template<>
class RunOnce<void>: public RunOnceBase
{
public:

    RunOnce() = default;

    /*
     * Schedule new task if it doesn't exists.
     */
    std::optional<Future> startOnceAsync(std::function<void()> task)
    {
        if (switchState(State::idle, State::inProgress))
            return startTaskInternal(std::move(task));
        return std::nullopt;
    }

    /*
     * Schedule new task if it doesn't exists or it is running now.
     * Multiple calls schedule only one task but it is guarantee that
     * new task is scheduled after `restartOnceAsync` call.
     */
    std::optional<Future> restartOnceAsync(
        std::function<void()> task)
    {
        if (switchState(State::inProgress, State::reinit))
            return std::nullopt;
        return startOnceAsync(std::move(task));
    }

private:
    std::atomic<State> m_state{State::idle};

    bool switchState(State from, State to)
    {
        return m_state.compare_exchange_strong(from, to);
    }

    Future startTaskInternal(std::function<void()> task)
    {
        return std::async(std::launch::async,
            [this, task = std::move(task)]()
            {
                while (true)
                {
                    task();
                    if (switchState(State::inProgress, State::idle))
                        break;
                    m_state = State::inProgress;
                }
            });
    }
};

} // namespace nx::utils
