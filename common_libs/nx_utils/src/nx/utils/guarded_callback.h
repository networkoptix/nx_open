#pragma once

#include <type_traits>
#include <utility>

#include <QtCore/QObject>
#include <QtCore/QPointer>

namespace nx::utils {

// For callbacks without a return value.
template<typename Callback>
auto guarded(QPointer<const QObject> guard, Callback&& callback)
{
    return
        [guard, callback = std::forward<Callback>(callback)](auto&&... args)
        {
            if (guard)
                callback(std::forward<std::remove_reference<decltype(args)>::type>(args)...);
        };
}

// For callbacks returning a value.
template<typename Callback, typename ResultType>
auto guarded(
    QPointer<const QObject> guard, const ResultType& defaultReturnValue, Callback&& callback)
{
    return
        [guard, defaultReturnValue, callback = std::forward<Callback>(callback)](auto&&... args)
        {
            return guard
                ? callback(std::forward<std::remove_reference<decltype(args)>::type>(args)...)
                : defaultReturnValue;
        };
}

} // namespace nx::utils
