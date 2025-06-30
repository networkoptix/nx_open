// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "request_rerunner.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QThread>

#include <nx/utils/log/assert.h>

namespace nx::vms::common {

nx::coro::Task<std::optional<nx::network::http::AuthToken>> RequestRerunner::getNewToken(
    nx::vms::common::SessionTokenHelperPtr helper)
{
    NX_ASSERT(QThread::currentThread() == qApp->thread());

    if (!hasRequestsToRerun())
    {
        // TODO: Guard against RequestsRerunner destruction.
        QMetaObject::invokeMethod(
            qApp,
            [this, helper]()
            {
                const auto token = helper->refreshSession();
                rerunRequests(token);
            },
            Qt::QueuedConnection);
    }

    auto newToken = co_await receiveToken();

    co_return newToken;
}

} // namespace nx::vms::common
