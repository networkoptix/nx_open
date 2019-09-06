#pragma once

#include <chrono>
#include <optional>
#include <functional>

#include <nx/utils/elapsed_timer.h>

namespace nx::utils {

template<typename Result, typename... Args>
class FrequencyRestrictedCallBase
{
public:
    FrequencyRestrictedCallBase(
        std::function<Result(Args...)> func,
        std::chrono::milliseconds minCallInterval)
        :
        m_func(std::move(func)),
        m_minCallInterval(std::move(minCallInterval))
    {
        m_timer.invalidate();
    }

protected:
    nx::utils::ElapsedTimer m_timer;
    std::function<Result(Args...)> m_func;
    const std::chrono::milliseconds m_minCallInterval;
};

template<typename Result, typename... Args>
class FrequencyRestrictedCall: public FrequencyRestrictedCallBase<Result, Args...>
{
public:
    using FrequencyRestrictedCallBase<Result, Args...>::FrequencyRestrictedCallBase;

    template<typename... ArgsInternal>
    std::optional<Result> operator()(ArgsInternal&&... args)
    {
        if (this->m_timer.isValid() && this->m_timer.elapsed() < this->m_minCallInterval)
            return std::nullopt;

        auto result = this->m_func(std::forward<ArgsInternal>(args)...);
        this->m_timer.restart();
        return result;
    }
};

template<typename... Args>
class FrequencyRestrictedCall<void, Args...>: public FrequencyRestrictedCallBase<void, Args...>
{
public:
    using FrequencyRestrictedCallBase<void, Args...>::FrequencyRestrictedCallBase;

    template<typename... ArgsInternal>
    bool operator()(ArgsInternal&&... args)
    {
        if (this->m_timer.isValid() && this->m_timer.elapsed() < this->m_minCallInterval)
            return false;

        this->m_func(std::forward<ArgsInternal>(args)...);
        this->m_timer.restart();
        return true;
    }
};

} // namespace nx::utils
