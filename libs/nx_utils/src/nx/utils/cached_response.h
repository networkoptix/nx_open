// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <concepts>
#include <functional>
#include <map>
#include <memory>
#include <type_traits>

#include <nx/utils/elapsed_timer.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/thread/mutex.h>

namespace nx {

struct CachedResponseTimeouts
{
    std::chrono::seconds updateTimeout = std::chrono::minutes{5};
    std::chrono::seconds failedUpdateTimeout{1};
};

namespace detail {

template<typename Generator, typename Result, typename... CallArgs>
concept CompatibleArgs =
    std::invocable<Generator&, CallArgs...>&&
    std::same_as<std::invoke_result_t<Generator&, CallArgs...>, Result>;

template<typename T>
concept CacheableValue =
    std::default_initializable<T> &&
    std::copy_constructible<T> &&
    requires(const T& value) { { static_cast<bool>(value) } -> std::same_as<bool>; };

template<typename T>
concept CacheKey =
    std::copy_constructible<T> &&
    requires(const T& a, const T& b) { { a < b } -> std::convertible_to<bool>; };

} // namespace detail

template<typename Signature>
class CachedResponse;

template<detail::CacheableValue CachedValue, typename... Args>
class CachedResponse<CachedValue(Args...)>
{
public:
    using ValueGenerator = nx::MoveOnlyFunc<CachedValue(Args...)>;

public:
    explicit CachedResponse(ValueGenerator getter, CachedResponseTimeouts timeouts = CachedResponseTimeouts{})
        :
        m_getter(std::move(getter)),
        m_timeouts(timeouts)
    {
    }

    template<typename... CallArgs>
        requires detail::CompatibleArgs<ValueGenerator, CachedValue, CallArgs...>
    [[nodiscard]] CachedValue operator()(CallArgs&&... args)
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        updateIfNeededUnsafe(std::forward<CallArgs>(args)...);
        return m_value;
    }

    void invalidate()
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        m_timer.invalidate();
    }

    const CachedResponseTimeouts& timeouts() const { return m_timeouts; }

private:
    template<typename... CallArgs>
    void updateIfNeededUnsafe(CallArgs&&... args)
    {
        const auto timeout = m_value ? m_timeouts.updateTimeout : m_timeouts.failedUpdateTimeout;
        if (m_timer.hasExpired(timeout))
        {
            m_value = std::invoke(m_getter, std::forward<CallArgs>(args)...);
            m_timer.restart();
        }
    }

private:
    ValueGenerator m_getter;

    nx::Mutex m_mutex;
    nx::utils::ElapsedTimer m_timer;
    CachedValue m_value;
    CachedResponseTimeouts m_timeouts;
};

template<typename Key, typename Signature>
class ParameterizedCachedResponse;

template<detail::CacheKey Key, detail::CacheableValue CachedValue, typename... Args>
class ParameterizedCachedResponse<Key, CachedValue(Args...)>
{
public:
    using ValueGenerator = nx::MoveOnlyFunc<CachedValue(Args...)>;
    using KeyGenerator = nx::MoveOnlyFunc<Key(Args...)>;
    using SingleCachedResponse = CachedResponse<CachedValue(Args...)>;

public:
    explicit ParameterizedCachedResponse(
        ValueGenerator getter,
        KeyGenerator keyGenerator,
        CachedResponseTimeouts timeouts = CachedResponseTimeouts{})
        :
        m_getter(std::make_shared<ValueGenerator>(std::move(getter))),
        m_keyGenerator(std::move(keyGenerator)),
        m_timeouts(timeouts)
    {
    }

    template<typename... CallArgs>
        requires detail::CompatibleArgs<ValueGenerator, CachedValue, CallArgs...>
    CachedValue operator()(CallArgs&&... args)
    {
        auto cache = tryEmplace(std::invoke(m_keyGenerator, args...));

        // Mutex is used only for search and adding elements to map. Here it->second can be called
        // without lock because we never delete any element from map.
        // Adding some methods that delete elements will lead to necessity to use lock here
        // and it could decrease performance and reduce benefits of using shared context.
        return (*cache)(std::forward<CallArgs>(args)...);
    }

    void invalidate()
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        for ([[maybe_unused]] auto& [_, val]: m_values)
            val->invalidate();
    }

    template<typename... CallArgs>
        requires detail::CompatibleArgs<ValueGenerator, CachedValue, CallArgs...>
    void invalidate(CallArgs&&... args)
    {
        auto key = std::invoke(m_keyGenerator, std::forward<CallArgs>(args)...);
        NX_MUTEX_LOCKER lock(&m_mutex);
        if (auto it = m_values.find(key); it != m_values.end())
            return it->second->invalidate();
    }

private:
    auto tryEmplace(Key key)
    {
        NX_MUTEX_LOCKER lock(&m_mutex);

        if (auto it = m_values.find(key); it != m_values.end())
            return it->second;

        auto cache = std::make_shared<SingleCachedResponse>(
            [getter = m_getter](auto&&... args) -> CachedValue
            {
                return std::invoke(*getter, std::forward<decltype(args)>(args)...);
            },
            m_timeouts);

        auto [it, success] = m_values.try_emplace(std::move(key), std::move(cache));
        return it->second;
    }

private:
    std::shared_ptr<ValueGenerator> m_getter;
    KeyGenerator m_keyGenerator;
    nx::Mutex m_mutex;
    // ParameterizedCachedResponse semantically acts as a single getter invoked with
    // different arguments, so getter is shared across all per-key caches intentionally.
    // Mutable state inside the getter is therefore shared across all keys.
    std::map<Key, std::shared_ptr<SingleCachedResponse>> m_values;
    CachedResponseTimeouts m_timeouts;
};

} // namespace nx
