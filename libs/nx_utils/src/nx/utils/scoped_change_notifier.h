// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>

namespace nx::utils {

/**
 * Guard that calls the given callback at the scope exit if value returned by the provided function
 * wrapper at the scope exit differs to the one provided at the guard initialization. Useful for
 * implementing change notifications of properties calculated on-demand, i.e the ones which value
 * is not explicitly presented.
 */
template <typename T>
class [[nodiscard]] ScopedChangeNotifier
{
public:
    using ValueGetter = std::function<T()>;
    using NotificationCallback = std::function<void()>;

    explicit ScopedChangeNotifier(
        const ValueGetter& valueGetter,
        const NotificationCallback& notificationCallback)
        :
        m_valueGetter(valueGetter),
        m_notificationCallback(notificationCallback),
        m_initialValue(m_valueGetter())
    {
    }

    ScopedChangeNotifier() = delete;
    ScopedChangeNotifier(const ScopedChangeNotifier&) = delete;
    ScopedChangeNotifier& operator=(const ScopedChangeNotifier&) = delete;
    ~ScopedChangeNotifier()
    {
        if (m_initialValue != m_valueGetter())
            m_notificationCallback();
    }

private:
    const ValueGetter m_valueGetter;
    const NotificationCallback m_notificationCallback;
    const T m_initialValue;
};

} // namespace nx::utils
