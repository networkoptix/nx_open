// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/utils/thread/mutex.h>

#include "scope_guard.h"

namespace nx {

namespace detail {

struct QtDirectConnectScope
{
    Mutex mutex;
    bool isConnected = true;
};

} // namespace detail

/**
 * Connects the `signal` of the `object` to `handler` and returns a guard which should be kept to
 * maintain this connection. It is guaranteed that the `handler` is not running when the guard
 * is destroyed.
 */
template<typename Object, typename Signal, typename Handler>
[[nodiscard]] nx::utils::SharedGuardPtr qtDirectConnect(
    const Object& object, Signal signal, Handler handler)
{
    const auto scope = std::make_shared<detail::QtDirectConnectScope>();
    const auto connection = QObject::connect(
        object, signal,
        [scope, handler = std::move(handler)](auto... args) mutable
        {
            NX_MUTEX_LOCKER lock(&scope->mutex);
            if (scope->isConnected)
                handler(std::move(args)...);
        });

    return nx::utils::makeSharedGuard(
        [scope, connection]()
        {
            QObject::disconnect(connection);
            NX_MUTEX_LOCKER lock(&scope->mutex);
            scope->isConnected = false;
        });
}

} // namespace nx
