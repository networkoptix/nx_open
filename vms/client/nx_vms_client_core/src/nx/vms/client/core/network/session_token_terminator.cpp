// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "session_token_terminator.h"

#include <chrono>

#include <api/server_rest_connection.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/log/log.h>
#include <nx/utils/thread/mutex.h>

#include "remote_connection.h"

using namespace std::chrono;

namespace nx::vms::client::core {

namespace {

constexpr auto kTerminateTimeout = 3s;
constexpr auto kTerminateRefreshStep = 100ms;

} // namespace

struct SessionTokenTerminator::Private
{
    std::map<rest::Handle, RemoteConnectionPtr> runningRequests;
};

SessionTokenTerminator::SessionTokenTerminator(QObject* parent):
    QObject(parent),
    d(new Private())
{
}

SessionTokenTerminator::~SessionTokenTerminator()
{
    const auto start = std::chrono::high_resolution_clock::now();
    auto tokensLeft = d->runningRequests.size();

    while (tokensLeft > 0)
    {
        NX_DEBUG(this, "Waiting till %1 tokens are being terminated", tokensLeft);
        std::this_thread::sleep_for(kTerminateRefreshStep);

        qApp->processEvents(); //< Allow terminate callback to be processed.
        tokensLeft = d->runningRequests.size();
        if (std::chrono::high_resolution_clock::now() - start > kTerminateTimeout)
            break;
    }

    if (tokensLeft > 0)
        NX_WARNING(this, "%1 tokens are still not terminated", tokensLeft);
}

void SessionTokenTerminator::terminateToken(RemoteConnectionPtr connection)
{
    NX_ASSERT(QThread::currentThread() == this->thread(), "Method should be called in UI thread");

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
            rest::ServerConnection::ErrorOrEmpty /*requestResult*/)
        {
            if (!success)
                NX_WARNING(this, "Can not terminate authentication token");

            auto it = d->runningRequests.find(handle);
            if (it != d->runningRequests.cend())
                d->runningRequests.erase(it);
        });

    NX_DEBUG(this, "Terminate authentication token");

    auto handle = connection->serverApi()->sendRequest<rest::ServerConnection::ErrorOrEmpty>(
        /*helper*/ nullptr,
        nx::network::http::Method::delete_,
        QString("/rest/v1/login/sessions/") + credentials.authToken.value,
        nx::network::rest::Params(),
        /*body*/ {},
        std::move(callback),
        this->thread()); //< Response handler will be called in UI thread.

    if (handle != rest::Handle())
        d->runningRequests.emplace(handle, connection);
}

} // namespace nx::vms::client::core
