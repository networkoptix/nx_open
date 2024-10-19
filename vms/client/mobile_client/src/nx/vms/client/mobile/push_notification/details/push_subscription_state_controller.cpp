// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "push_subscription_state_controller.h"

#include <optional>

#include <context/context.h>
#include <nx/network/http/auth_tools.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/log/log.h>
#include <nx/utils/scope_guard.h>
#include <utils/common/delayed.h>

#include "push_api_helper.h"
#include "token_manager.h"

namespace {

using namespace nx::vms::client::mobile::details;

using Credentials = nx::network::http::Credentials;
using Callback = PushSettingsRemoteController::Callback;
using SubscribeCallback = std::function<void (bool success, const TokenData& data)>;

enum class RequestType
{
    internal,
    userObservable
};

/**
 * Initially we supported only Firebase provider for both platforms (Android and iOS).
 * Then was decided to support native APN provider for iOS and additionally support Baidu
 * provider for China.
 * This function changes push settings (if needed) to send resubscribe request to the cloud
 * and migrate to the appropriate provider.
 */
OptionalLocalPushSettings adoptSettingsForProviderMigration(
    OptionalLocalPushSettings settings,
    TokenManager* tokenManager)
{
    // No need to migrate if (because it will be done automatically next time):
    //  - it is first time user login;
    //  - push notifications are disabled;
    //  - user is explicitly logging in to the cloud now.
    //
    // In all other cases (user was logged in into the cloud with enabled push notifications) we
    // should check if push notifications provider should be updated.
    if (!settings || !settings->enabled || !settings->tokenData.isValid())
        return settings;

    if (tokenManager->providerType() != settings->tokenData.provider)
        settings->tokenData.reset(); // Force migration process to the correct provider.

    return settings;
}

Callback makeSafeCallback(Callback callback = Callback())
{
    return callback
        ? callback
        : [](bool /*success*/) {};
}

using Request = std::function<
    void (const HttpClientPtr& /*client*/, PushApiHelper::Callback /*callback*/)>;

Request createUnsubscribeRequest(
    const Credentials& credentials,
    const TokenData& tokenData)
{
    return
        [credentials, tokenData]
            (const HttpClientPtr& client, PushApiHelper::Callback callback)
        {
            return PushApiHelper::unsubscribe(
                credentials,
                tokenData,
                client,
                callback);
        };
}

Request createSubscribeRequest(
    const Credentials& credentials,
    const SystemSet& systems,
    const TokenData& tokenData)
{
    return
        [credentials, systems, tokenData]
            (const HttpClientPtr& client, PushApiHelper::Callback callback)
        {
            return PushApiHelper::subscribe(
                credentials,
                tokenData,
                systems,
                client,
                callback);
        };
}

} // namespace

namespace nx::vms::client::mobile::details {

struct PushSettingsRemoteController::Private: public QObject
{
    PushSettingsRemoteController* const q;

    Credentials credentials;
    OptionalLocalPushSettings settings;

    RequestType requestType = RequestType::internal;
    HttpClientPtr requestClient;

    std::unique_ptr<TokenManager> tokenManager = std::make_unique<TokenManager>();

    Private(PushSettingsRemoteController* q);

    void tryUpdateRemoteSettings(
        bool userObservableAction,
        const LocalPushSettings& value,
        Callback callback);

    void setCredentials(const Credentials& value);

    void resetCurrentRequest();

    void subscribe(
        RequestType type,
        const Credentials& targetCredentials,
        const SystemSet& systems,
        const TokenData& lastKnownTokenData,
        SubscribeCallback callback);

    void unsubscribe(
        RequestType type,
        const Credentials& targetCredentials,
        const TokenData& tokenData,
        Callback callback);

private:
    bool isRequestOutdated(const HttpClientPtr& client) const;

    void setRequestClient(
        const HttpClientPtr& client,
        RequestType source);

    void sendRequest(
        const HttpClientPtr& client,
        Request request,
        Callback callback,
        int triesCount = 1);
};

PushSettingsRemoteController::Private::Private(PushSettingsRemoteController* q):
    q(q)
{
}

void PushSettingsRemoteController::Private::tryUpdateRemoteSettings(
    bool userObservableAction,
    const LocalPushSettings& value,
    Callback callback)
{
    if (credentials.authToken.empty())
    {
        callback(true);
        return;
    }

    const auto requestType = userObservableAction
        ? RequestType::userObservable
        : RequestType::internal;

    const auto tokenData = settings
        ? settings->tokenData
        : TokenData::makeEmpty();

    if (value.enabled)
    {
        subscribe(requestType, credentials, value.systems, tokenData,
            nx::utils::guarded(this,
            [this, callback, value](bool success, const TokenData& tokenData)
            {
                if (success)
                {
                    q->replaceConfirmedSettings(
                        LocalPushSettings::makeCopyWithTokenData(value, tokenData));
                }

                if (callback)
                    callback(success);
            }));

        return;
    }

    if (!NX_ASSERT(tokenData.isValid(), "Can't update remote settings with invalid token!"))
        return;

    unsubscribe(requestType, credentials, tokenData,
        nx::utils::guarded(this,
        [this, value, callback](bool /*success*/)
        {
            // Unsbuscription is optional, so it is always successful.
            q->replaceConfirmedSettings(value);
            if (callback)
                callback(true);
        }));
}

void PushSettingsRemoteController::Private::setCredentials(const Credentials& value)
{
    if (credentials == value)
        return;

    credentials = value;
    emit q->loggedInChanged();
}

void PushSettingsRemoteController::Private::resetCurrentRequest()
{
    setRequestClient(HttpClientPtr(), RequestType::internal);
}

void PushSettingsRemoteController::Private::subscribe(
    RequestType type,
    const Credentials& targetCredentials,
    const SystemSet& systems,
    const TokenData& lastKnownTokenData,
    SubscribeCallback callback)
{
    if (q->userUpdateInProgress())
        return;

    setRequestClient(PushApiHelper::createClient(), type);

    const auto requestTokenCallback = nx::utils::guarded(this,
        [this, targetCredentials, systems, callback, client = requestClient]
            (const TokenData& tokenData)
        {
            if (isRequestOutdated(client))
                return;

            if (!tokenData.isValid())
            {
                callback(false, TokenData::makeEmpty());
                resetCurrentRequest(); // < We didn't start request so we have to reset it manually.
                return;
            }

            const auto subscribeRequest =
                createSubscribeRequest(targetCredentials, systems, tokenData);

            const auto subscribeCallbackAdapter =
                [callback, tokenData](bool success) { callback(success, tokenData); };
            sendRequest(client, subscribeRequest, subscribeCallbackAdapter);
        });

    // Valid token data means that we are just turning notifications on after they've been
    // disabled by the same cloud user. In this case we don't ask for the forced token update.
    // In case of invalid last token we ask for the renewal because it is possible only with
    // the new log in operation of cloud user.
    // Token renewal on new cloud user login is needed for the safety to make sure he won't
    // receive any old push notification sent for another user.
    tokenManager->requestToken(requestTokenCallback, /*forceUpdate*/ !lastKnownTokenData.isValid());
}

void PushSettingsRemoteController::Private::unsubscribe(
    RequestType type,
    const Credentials& targetCredentials,
    const TokenData& tokenData,
    Callback callback)
{
    if (q->userUpdateInProgress())
        return;

    setRequestClient(PushApiHelper::createClient(), type);

    const auto deleteTokenCallback = nx::utils::guarded(this,
        [this, targetCredentials, tokenData, callback, client = requestClient](bool /*success*/)
        {
            if (isRequestOutdated(client))
                return;

            const auto unsubscribeRequest =
                createUnsubscribeRequest(targetCredentials, tokenData);
            sendRequest(client, unsubscribeRequest, callback);
        });

    tokenManager->resetToken(deleteTokenCallback);
}

bool PushSettingsRemoteController::Private::isRequestOutdated(const HttpClientPtr& client) const
{
    return !client || client != requestClient;
}

void PushSettingsRemoteController::Private::setRequestClient(
    const HttpClientPtr& client,
    RequestType type)
{
    if (client == requestClient || (client && requestType == RequestType::userObservable))
        return;

    requestClient = client;
    requestType = type;
    q->userUpdateInProgressChanged();
}

void PushSettingsRemoteController::Private::sendRequest(
    const HttpClientPtr& client,
    Request request,
    Callback callback,
    int triesCount)
{
    if (isRequestOutdated(client))
        return;

    const auto requestCallback = nx::utils::guarded(this,
        [this, triesCount, client, request, callback](bool success)
        {
            if (isRequestOutdated(client))
                return;

            if (success || triesCount <= 1)
            {
                callback(success);
                resetCurrentRequest();
                return;
            }

            sendRequest(client, request, callback, triesCount - 1);
        });

    request(client, requestCallback);
}

//--------------------------------------------------------------------------------------------------

PushSettingsRemoteController::PushSettingsRemoteController(QObject* parent):
    base_type(parent),
    d(new Private{this})
{
}

PushSettingsRemoteController::~PushSettingsRemoteController()
{
}

bool PushSettingsRemoteController::userUpdateInProgress() const
{
    return d->requestClient && d->requestType == RequestType::userObservable;
}

const nx::network::http::Credentials& PushSettingsRemoteController::credentials() const
{
    return d->credentials;
}

OptionalLocalPushSettings PushSettingsRemoteController::confirmedSettings() const
{
    return d->settings;
}

void PushSettingsRemoteController::logIn(
    const nx::network::http::Credentials& credentials,
    const OptionalLocalPushSettings& confirmedSettings,
    Callback callback)
{
    if (d->credentials == credentials)
        return;


    if (d->credentials.username != credentials.username)
        logOut();

    d->setCredentials(credentials);
    d->settings = adoptSettingsForProviderMigration(confirmedSettings, d->tokenManager.get());
    d->resetCurrentRequest(); // We cancel previos request anyway.

    if (!d->settings)
    {
        // In case we don't have any settings stored we just try to subscribe to all systems
        // by default.
        static const LocalPushSettings kSubscribeToAllSystems(
            /*enabled*/ true, allSystemsModeValue(), TokenData::makeEmpty());
        d->tryUpdateRemoteSettings(false /*userObservedAction*/, kSubscribeToAllSystems, callback);
    }
    else if (d->settings->enabled && !d->settings->tokenData.isValid())
    {
        // Try to resubscribe after user is logged in into the cloud or if provider migration
        // is needed.
        d->tryUpdateRemoteSettings(false /*userObservedAction*/, *d->settings, callback);
    }
    else
    {
        // We just do nothing if user disabled notifications.
    }
}

void PushSettingsRemoteController::logOut()
{
    if (d->credentials.username.empty())
        return;

    if (!d->settings)
        return;

    const auto previosSettings = d->settings;
    const auto previosCredentials = d->credentials;

    // Since unsubscribe request placed below could be replaced by another one (theoretically)
    // we explicitly clear stored token data as soon as we receive logout request.
    replaceConfirmedSettings(
        LocalPushSettings::makeCopyWithTokenData(*d->settings, TokenData::makeEmpty()));

    d->setCredentials(Credentials());
    d->settings = OptionalLocalPushSettings();

    // We try to unsubscribe user, but actually it is not mandatory. Just a try.
    if (!previosSettings->tokenData.isValid() || !previosSettings->enabled)
        return;

    d->unsubscribe(
        RequestType::internal,
        previosCredentials,
        previosSettings->tokenData,
        makeSafeCallback());
}

void PushSettingsRemoteController::replaceConfirmedSettings(const LocalPushSettings& settings)
{
    if (d->settings && *d->settings == settings)
        return;

    d->settings = settings;

    emit confirmedSettingsChanged();
}

void PushSettingsRemoteController::tryUpdateRemoteSettings(
    const LocalPushSettings& value,
    Callback callback)
{
    d->tryUpdateRemoteSettings(/*userObservableAction*/ true, value, callback);
}

bool PushSettingsRemoteController::loggedIn() const
{
    return !d->credentials.authToken.empty();
}

} // namespace nx::vms::client::mobile::details
