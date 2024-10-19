// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "token_manager.h"

#include <QtCore/QTimer>
#include <QtCore/QThread>

#include <nx/utils/log/log.h>
#include <nx/utils/scoped_connections.h>
#include <utils/common/delayed.h>

#include "token_data_watcher.h"
#include "token_data_provider.h"

namespace nx::vms::client::mobile::details {

struct TokenManager::Private: public QObject
{
    using OperationCallback = std::function<void ()>;
    using CompletionCheck = std::function<bool ()>;
    using InitOperation = std::function<bool ()>;

    TokenDataWatcher& watcher;

    TokenDataProvider::Pointer provider;

    int remainingTriesCount = 0;
    nx::utils::ScopedConnections operationConnections;
    OperationCallback operationCallback;
    CompletionCheck completionCheck;
    QTimer operationTimer;

    Private();

    void startOperation(
        const InitOperation& init,
        const CompletionCheck& check,
        const OperationCallback& callback);

    void checkOperation();
    void completeCurrentRequest();
};

TokenManager::Private::Private():
    watcher(TokenDataWatcher::instance()),
    provider(TokenDataProvider::create())
{
    static const std::chrono::milliseconds kTokenCheckInterval(500);
    operationTimer.setInterval(kTokenCheckInterval);
    operationTimer.setSingleShot(true);
}

void TokenManager::Private::startOperation(
    const InitOperation& init,
    const CompletionCheck& check,
    const OperationCallback& callback)
{
    remainingTriesCount = 40;
    operationCallback = callback;
    completionCheck = check;

    if (!init())
    {
        // Provider can't request token, something went wrong, closing request.
        completeCurrentRequest();
        return;
    }

    operationConnections
        << connect(&watcher, &TokenDataWatcher::tokenDataChanged,
            this, &TokenManager::Private::completeCurrentRequest);
    operationConnections
        << connect(&operationTimer, &QTimer::timeout,
            this, &TokenManager::Private::checkOperation);

    // Checks if operation has completed already, otherwise starts timer and checks
    // the result periodically.
    checkOperation();
}

void TokenManager::Private::checkOperation()
{
    NX_ASSERT(remainingTriesCount > 0,
        "Something went wrong, tries count should be greater than 0!");

    NX_ASSERT(completionCheck, "Please fill completion check functor!");
    if (!completionCheck || completionCheck() || --remainingTriesCount <= 0)
        completeCurrentRequest();
    else
        operationTimer.start();
}

void TokenManager::Private::completeCurrentRequest()
{
    operationTimer.stop();
    remainingTriesCount = 0;

    const auto callback = operationCallback;
    operationCallback = OperationCallback();
    completionCheck = CompletionCheck();
    operationConnections.reset();

    if (callback)
        callback();
}

//-------------------------------------------------------------------------------------------------

TokenManager::TokenManager(QObject* parent):
    base_type(parent),
    d(new Private())
{
}

TokenManager::~TokenManager()
{
}

void TokenManager::requestToken(
    RequestCallback callback,
    bool forceUpdate)
{
    const auto requestTokenImpl =
        [this, safeCallback = threadGuarded(callback)]()
        {
            if (!NX_ASSERT(d->provider))
            {
                safeCallback(TokenData::makeEmpty());
                return;
            }

            d->watcher.resetData(); //< Reset token data to make sure that's not the old one.

            const auto initOperation =
                [this]()
                {
                    return d->provider->requestTokenDataUpdate();
                };
            const auto completionCheck =
                [this]()
                {
                    return d->watcher.hasData();
                };
            const auto operationCallback =
                [this, safeCallback]()
                {
                    if (d->watcher.hasData())
                    {
                        NX_DEBUG(this, "Token data has been received.");
                        safeCallback(d->watcher.data());
                    }
                    else
                    {
                        NX_DEBUG(this, "Can't receive token data!");
                        safeCallback(TokenData::makeEmpty());
                    }
                };

            d->startOperation(initOperation, completionCheck, operationCallback);
        };

    if (forceUpdate)
        resetToken([requestTokenImpl](bool /*success*/) { requestTokenImpl(); });
    else
        requestTokenImpl();
}

void TokenManager::resetToken(ResetCallback callback)
{
    d->completeCurrentRequest();

    if (!NX_ASSERT(d->provider))
    {
        callback(false);
        return;
    }

    if (!d->watcher.hasData())
    {
        // Since we rely on filled token data before token invalidation and use it as a condition
        // we set it to some fake value if it is empty for now.
        d->watcher.setData(TokenData::makeInternal());
    }

    const auto initOperation = [this]() { return d->provider->requestTokenDataReset(); };
    const auto completionCheck = [this]() { return !d->watcher.hasData(); };
    const auto operationCallback =
        [this, safeCallback = threadGuarded(callback)]()
        {
            const bool success = !d->watcher.hasData();
            NX_DEBUG(this,
                QString("Reset token request has been %1").arg(success ? "completed" : "failed"));
            d->watcher.resetData();
            safeCallback(success);
        };

    d->startOperation(initOperation, completionCheck, operationCallback);
}

TokenProviderType TokenManager::providerType() const
{
    return d->provider->type();
}

} // namespace nx::vms::client::mobile::details
