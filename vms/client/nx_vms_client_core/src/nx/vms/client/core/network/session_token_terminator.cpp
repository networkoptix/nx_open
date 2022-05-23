// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "session_token_terminator.h"

#include <chrono>

#include <QtCore/QTimer>

#include <api/server_rest_connection.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/log/log.h>

#include "remote_connection.h"

using namespace std::chrono;

namespace nx::vms::client::core {

namespace {

constexpr auto kCleanupInterval = 10s;
constexpr auto kTerminateTimeout = 3s;
constexpr auto kTerminateRefreshStep = 100ms;

} // namespace

struct SessionTokenTerminator::Private
{
    std::map<rest::Handle, RemoteConnectionPtr> runningRequests;
    std::vector<RemoteConnectionPtr> completedRequests;
    nx::Mutex mutex;
};

SessionTokenTerminator::SessionTokenTerminator(QObject* parent):
    QObject(parent),
    d(new Private())
{
    auto cleanupTimer = new QTimer(this);
    cleanupTimer->setSingleShot(false);
    cleanupTimer->setInterval(kCleanupInterval);
    cleanupTimer->callOnTimeout(
        [this]
        {
            NX_MUTEX_LOCKER lock(&d->mutex);
            d->completedRequests.clear();
        });
    cleanupTimer->start();
}

SessionTokenTerminator::~SessionTokenTerminator()
{
    auto calculateTokensLeft =
        [this]
        {
            NX_MUTEX_LOCKER lock(&d->mutex);
            return d->runningRequests.size();
        };

    auto start = std::chrono::high_resolution_clock::now();
    int tokensLeft = calculateTokensLeft();
    while (tokensLeft > 0)
    {
        NX_DEBUG(this, "Waiting till %1 tokens are being terminated", tokensLeft);
        std::this_thread::sleep_for(kTerminateRefreshStep);
        tokensLeft = calculateTokensLeft();
        if (std::chrono::high_resolution_clock::now() - start > kTerminateTimeout)
            break;
    }

    if (tokensLeft > 0)
        NX_WARNING(this, "%1 tokens are still not terminated", tokensLeft);
}

void SessionTokenTerminator::terminateTokenAsync(RemoteConnectionPtr connection)
{
    if (!NX_ASSERT(connection))
        return;

    if (connection->userType() == nx::vms::api::UserType::cloud)
        return;

    const auto credentials = connection->credentials();
    const bool canDeleteToken = !credentials.authToken.empty()
        && credentials.authToken.isBearerToken();
    if (!canDeleteToken)
        return;

    auto callback = nx::utils::guarded(this,
        [this](
            bool success,
            rest::Handle handle,
            rest::ServerConnection::EmptyResponseType /*requestResult*/)
        {
            if (!success)
                NX_WARNING(this, "Can not terminate authentication token");

            NX_MUTEX_LOCKER lock(&d->mutex);
            auto it = d->runningRequests.find(handle);
            if (it != d->runningRequests.cend())
            {
                // Make sure connection is not destroyed while we are in the callback.
                d->completedRequests.push_back(it->second);
                d->runningRequests.erase(it);
            }
        });

    NX_DEBUG(this, "Terminate authentication token");
    auto handle = connection->serverApi()->deleteEmptyResult(
        QString("/rest/v1/login/sessions/") + credentials.authToken.value,
        nx::network::rest::Params(),
        callback);

    NX_MUTEX_LOCKER lock(&d->mutex);
    if (handle != rest::Handle())
        d->runningRequests.emplace(handle, connection);
}

} // namespace nx::vms::client::core
