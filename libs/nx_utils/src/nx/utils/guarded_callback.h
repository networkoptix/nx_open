#pragma once

#include <type_traits>
#include <utility>

#include <QtCore/QObject>
#include <QtCore/QPointer>

namespace nx::utils {

/**
 * These functions construct a wrapper functor around provided callback and a guard QObject.
 * The wrapper won't call the callback if the guard is destroyed by the moment of invokation.
 *
 * The wrapper is a variadic generic lambda, it can be used directly where non-generic function
 * is expected, but explicit specialization must be ensured if it's passed as a generic argument,
 * for example:
 *     QMenu::addAction<std::function<void()>>("Name", guarded([]() {}));
 * or
 *     QMenu::addAction("Name", std::function<void()>(guarded([]() {})));
 */

// For callbacks without a return value.
template<typename Callback>
auto guarded(const QObject* guard, Callback&& callback)
{
    return
        [guard = QPointer<const QObject>(guard),
            callback = std::forward<Callback>(callback)](auto&&... args)
        {
            if (guard)
                callback(std::forward<decltype(args)>(args)...);
        };
}

// For callbacks returning a value.
template<typename Callback, typename ResultType>
auto guarded(const QObject* guard, const ResultType& defaultReturnValue, Callback&& callback)
{
    return
        [guard = QPointer<const QObject>(guard),
            defaultReturnValue,
            callback = std::forward<Callback>(callback)](auto&&... args)
        {
            return guard
                ? callback(std::forward<decltype(args)>(args)...)
                : defaultReturnValue;
        };
}

} // namespace nx::utils
