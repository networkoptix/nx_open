// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "session_manager.h"

#include <chrono>

#include <QtQml/QtQml>

#include <network/system_helpers.h>
#include <nx/network/http/http_types.h>
#include <nx/utils/log/log.h>
#include <nx/utils/url.h>
#include <nx/vms/client/core/application_context.h>
#include <nx/vms/client/core/network/cloud_status_watcher.h>
#include <nx/vms/client/core/network/connection_info.h>
#include <nx/vms/client/core/network/credentials_manager.h>
#include <nx/vms/client/mobile/system_context.h>
#include <nx/vms/common/system_settings.h>
#include <utils/common/delayed.h>

#include "ui_messages.h"

namespace nx::vms::client::mobile {

struct SessionManager::Private: public QObject
{
    SessionManager* const q;
    Session::Holder session;
    nx::Uuid nextResetActionId;
    bool hasConnectedSession = false;

    bool reconnecting = false; //< Shows if session is reconnecting now.
    bool delayedReconnecting = false; //< Shows if session is reconnecting after some delay.
    QTimer reconnectingDelayTimer;

    Private(SessionManager* q);

    void startSession(
        const Session::ConnectionData& data,
        const QString& supposedSystemName,
        session::ConnectionCallback&& connectionCallback);
    void resetSession();
    void handleSessionChanged();

    void setDelayedReconnecting(bool value);
    void updateRestoringState();
    void updateHasConnectedSession();

    session::ConnectionCallback wrapCallback(QJSValue&& jsCallback = QJSValue());
    session::ConnectionCallback wrapCallback(session::ConnectionCallback&& connectionCallback);
};

SessionManager::Private::Private(SessionManager* q):
    q(q)
{
    using namespace std::chrono;
    reconnectingDelayTimer.setInterval(20s);
    reconnectingDelayTimer.setSingleShot(false);
    connect(&reconnectingDelayTimer, &QTimer::timeout,
        this, [this]() { setDelayedReconnecting(true); });

    connect(q, &SessionManager::stateChanged,
        this, &SessionManager::Private::updateHasConnectedSession);
}

void SessionManager::Private::startSession(
    const Session::ConnectionData& data,
    const QString& supposedSystemName,
    session::ConnectionCallback&& connectionCallback)
{
    resetSession();

    // TODO: #sivanov Make this class SystemContextAware.
    session = Session::create(
        q->systemContext(),
        data,
        supposedSystemName,
        std::move(connectionCallback));

    const auto handleStateChanged =
        [this]()
        {
            updateRestoringState();
            emit q->stateChanged();
        };

    connect(session.get(), &Session::connectionStateChanged, q, handleStateChanged);
    connect(session.get(), &Session::restoringConnectionChanged, q, handleStateChanged);
    connect(session.get(), &Session::connectedServerVersionChanged,
        q, &SessionManager::connectedServerVersionChanged);
    connect(session.get(), &Session::systemNameChanged,
        q, &SessionManager::systemNameChanged);
    connect(session.get(), &Session::addressChanged,
        q, &SessionManager::sessionHostChanged);
    connect(session.get(), &Session::isCloudSessionChanged,
        q, &SessionManager::sessionHostChanged);
    connect(session.get(), &Session::parametersChanged,
        q, &SessionManager::sessionParametersChanged);
    connect(session.get(), &Session::restored,
        q, &SessionManager::sessionRestored);

    connect(session.get(), &Session::finishedWithError, this,
        [this](core::RemoteConnectionErrorCode errorCode)
        {
            nextResetActionId = nx::Uuid::createUuid();
            const auto resetSessionAction =
                [this, errorCode, actionId = nextResetActionId]()
                {
                    if (actionId != nextResetActionId)
                    {
                        NX_DEBUG(this, "Reset session action is outdated");
                        return;
                    }

                    const auto lastSession = session;
                    resetSession();
                    emit q->sessionFinishedWithError(lastSession, errorCode);
                };

            executeLater(resetSessionAction, this);
        });

    handleSessionChanged();
}

void SessionManager::Private::resetSession()
{
    nextResetActionId = nx::Uuid();
    session.reset();

    handleSessionChanged();
}

void SessionManager::Private::handleSessionChanged()
{
    q->systemContext()->globalSettings()->synchronizeNow();

    updateRestoringState();

    emit q->hasSessionChanged();
    emit q->stateChanged();
    emit q->sessionHostChanged();
    emit q->systemNameChanged();
    emit q->sessionHostChanged();
    emit q->connectedServerVersionChanged();
    emit q->hasReconnectingSessionChanged();
    emit q->hasConnectedSessionChanged();
}

void SessionManager::Private::setDelayedReconnecting(bool value)
{
    if (delayedReconnecting == value)
        return;

    delayedReconnecting = value;
    NX_DEBUG(this, "setDelayedReconnecting(): delayed reconnecting value is  set to <%1>", value);
    emit q->hasReconnectingSessionChanged();
}

void SessionManager::Private::updateRestoringState()
{
    NX_DEBUG(this, "updateRestoringState(): start");
    const bool currentlyReconnecting =
        [this]()
        {
            if (!session)
                return false;

            return session->connectionState() == Session::ConnectionState::reconnecting
                || session->restoringConnection();
        }();

    NX_DEBUG(this, "updateRestoringState(): currently reconnecting: <%1>", currentlyReconnecting);

    if (currentlyReconnecting == reconnecting)
        return;

    reconnecting = currentlyReconnecting;
    NX_DEBUG(this, "updateRestoringState(): reconnecting is changed to <%1>", reconnecting);

    if (!reconnecting)
    {
        NX_DEBUG(this, "updateRestoringState(): stopping delay timer");
        reconnectingDelayTimer.stop();
        setDelayedReconnecting(false);
    }
    else if (!reconnectingDelayTimer.isActive())
    {
        NX_DEBUG(this, "updateRestoringState(): starting delay timer");
        reconnectingDelayTimer.start();
    }
    else
    {
        NX_ASSERT(false, "Timer should not be active here");
    }

    NX_DEBUG(this, "updateRestoringState(): end");
}

void SessionManager::Private::updateHasConnectedSession()
{
    const bool value = session
        && session->connectionState() == Session::ConnectionState::ready;

    if (value == hasConnectedSession)
        return;

    hasConnectedSession = value;
    emit q->hasConnectedSessionChanged();
}

session::ConnectionCallback SessionManager::Private::wrapCallback(QJSValue&& jsCallback)
{
    return
        [this, callback = std::move(jsCallback)](
            std::optional<core::RemoteConnectionErrorCode> errorCode) mutable
        {
            if (!errorCode)
                emit q->sessionStartedSuccessfully();

            if (callback.isCallable())
            {
                ConnectionStatus status = ConnectionStatus::SuccessConnectionStatus;
                if (errorCode)
                {
                    switch (*errorCode)
                    {
                        case core::RemoteConnectionErrorCode::userIsLockedOut:
                            status = UserTemporaryLockedOutConnectionStatus;
                            break;
                        case core::RemoteConnectionErrorCode::unauthorized:
                            status = UnauthorizedConnectionStatus;
                            break;
                        case core::RemoteConnectionErrorCode::versionIsTooLow:
                            status = IncompatibleVersionConnectionStatus;
                            break;
                        case core::RemoteConnectionErrorCode::sessionExpired:
                            status = LocalSessionExiredStatus;
                            break;
                        default:
                            status = OtherErrorConnectionStatus;
                            break;
                    }
                }

                const QString connectionErrorText = errorCode
                    ? UiMessages::getConnectionErrorText(*errorCode)
                    : QString();
                callback.call(QJSValueList{status, connectionErrorText});
            }
        };
}

session::ConnectionCallback SessionManager::Private::wrapCallback(
    session::ConnectionCallback&& connectionCallback)
{
    return
        [this, callback = std::move(connectionCallback)](
            std::optional<core::RemoteConnectionErrorCode> errorCode)
        {
            if (!errorCode)
                emit q->sessionStartedSuccessfully();

            if (callback)
                callback(errorCode);
        };
}

//-------------------------------------------------------------------------------------------------

void SessionManager::registerQmlType()
{
    qmlRegisterUncreatableType<SessionManager>("nx.vms.client.mobile", 1, 0, "SessionManager",
        "Cannot create an instance of SessionManager");
}

SessionManager::SessionManager(SystemContext* context, QObject* parent):
    base_type(parent),
    SystemContextAware(context),
    d(new Private(this))
{
    NX_DEBUG(this, "SessionManager(): created");
}

SessionManager::~SessionManager()
{
    d->resetSession();
}

void SessionManager::startSessionWithCredentials(
    const nx::utils::Url& url,
    const nx::network::http::Credentials& credentials,
    session::ConnectionCallback&& callback)
{
    NX_DEBUG(this, "startSessionWithCredentials(): called");
    d->startSession(Session::LocalConnectionData{url, credentials}, /*supposedSystemName*/ {},
        d->wrapCallback(std::move(callback)));
    NX_DEBUG(this, "startSessionWithCredentials(): end");
}

void SessionManager::startCloudSession(
    const QString& cloudSystemId,
    const QString& supposedSystemName)
{
    NX_DEBUG(this, "startCloudSession(): called, supposed system name is <%1>", supposedSystemName);
    d->startSession(Session::CloudConnectionData{cloudSystemId, /*digestCredentials*/ {}},
        supposedSystemName, d->wrapCallback());
    NX_DEBUG(this, "startCloudSession(): end()");
}

void SessionManager::startDigestCloudSession(
    const QString& cloudSystemId,
    const QString& password,
    const QString& supposedSystemName)
{
    using namespace nx::vms::client::core;

    NX_DEBUG(this, "startDigestCloudSession(): called, supposed system name is <%1>",
        supposedSystemName);

    if (!NX_ASSERT(qnCloudStatusWatcher->status() != CloudStatusWatcher::LoggedOut))
    {
        NX_DEBUG(this, "startDigestCloudSession(): unexpected behavior, please log in to the cloud");
        return;
    }

    const nx::network::http::Credentials digestCredentials = {
        qnCloudStatusWatcher->cloudLogin().toStdString(),
        {password.toStdString(), nx::network::http::AuthTokenType::password}};

    d->startSession(Session::CloudConnectionData{cloudSystemId, digestCredentials},
        supposedSystemName, d->wrapCallback());
    NX_DEBUG(this, "startDigestCloudSession(): end");
}

void SessionManager::startCloudSession(
    const QString& cloudSystemId,
    const std::optional<nx::network::http::Credentials>& digestCredentials,
    session::ConnectionCallback&& callback)
{
    NX_DEBUG(this, "startCloudSession(): called");
    d->startSession(Session::CloudConnectionData{cloudSystemId, digestCredentials},
        /*supposedSystemName*/ {}, d->wrapCallback(std::move(callback)));
    NX_DEBUG(this, "startCloudSession(): end");
}

bool SessionManager::startSessionWithStoredCredentials(
    const nx::utils::Url& url,
    const nx::Uuid& localSystemId,
    const QString& user,
    QJSValue callback)
{
    NX_DEBUG(this, "startSessionWithStoredCredentials(): called");
    const auto storedCredentials = nx::vms::client::core::CredentialsManager::credentials(
        localSystemId, user.toStdString());

    if (!storedCredentials)
    {
        NX_DEBUG(this, "startSessionWithStoredCredentials(): can't find credentials");
        return false;
    }

    d->startSession(Session::LocalConnectionData{url, storedCredentials.value()},
        /*supposedSystemName*/ {}, d->wrapCallback(std::move(callback)));
    NX_DEBUG(this, "startSessionWithStoredCredentials(): end");
    return true;
}

void SessionManager::startSession(
    const nx::utils::Url& url,
    const QString& user,
    const QString& password,
    const QString& supposedSystemName,
    QJSValue callback)
{
    NX_DEBUG(this, "startSession(): called, supposed system name is <%1>", supposedSystemName);

    const nx::network::http::Credentials credentials(user.toStdString(),
        nx::network::http::PasswordAuthToken(password.toStdString()));
    d->startSession(Session::LocalConnectionData{url, credentials}, supposedSystemName,
        d->wrapCallback(std::move(callback)));
    NX_DEBUG(this, "startSession(): end");
}

void SessionManager::startSessionByUrl(
    const nx::utils::Url& url,
    session::ConnectionCallback&& callback)
{
    NX_DEBUG(this, "startSessionByUrl(): called");
    const auto& user = url.userName();
    const auto& password = url.password();
    if (!NX_ASSERT(!user.isEmpty() && !password.isEmpty(),
        "User name and password should be specified!"))
    {
        if (callback)
            callback(core::RemoteConnectionErrorCode::internalError);
        return;
    }

    const nx::network::http::Credentials credentials(user.toStdString(),
        nx::network::http::PasswordAuthToken(password.toStdString()));
    d->startSession(Session::LocalConnectionData{url, credentials}, /*supposedSystemName*/ {},
        d->wrapCallback(std::move(callback)));

    NX_DEBUG(this, "startSessionByUrl(): end");
}

void SessionManager::stopSession()
{
    NX_DEBUG(this, "stopSession(): called, session is <%1>", d->session);

    d->resetSession();
    emit sessionStoppedManually();
}

bool SessionManager::hasSession() const
{
    return !!d->session;
}

bool SessionManager::hasConnectingSession() const
{
    return d->session && d->session->connectionState() == Session::ConnectionState::connecting;
}

bool SessionManager::hasAwaitingResourcesSession() const
{
    return d->session
        && d->session->connectionState() == Session::ConnectionState::loading;
}

bool SessionManager::hasActiveSession() const
{
    if (!d->session)
        return false;

    return d->session->wasConnected()
        || d->session->connectionState() > Session::ConnectionState::loading;
}

bool SessionManager::hasConnectedSession() const
{
    return d->hasConnectedSession;
}

bool SessionManager::hasReconnectingSession() const
{
    return d->session && d->delayedReconnecting;
}

void SessionManager::setSuspended(bool suspended)
{
    if (d->session)
        d->session->setSuspended(suspended);
}

QString SessionManager::sessionHost() const
{
    if (!d->session)
        return QString();

    return QString::fromStdString(d->session->address().toString());
}

QString SessionManager::systemName() const
{
    return d->session
        ? d->session->systemName()
        : QString();
}

nx::utils::SoftwareVersion SessionManager::connectedServerVersion() const
{
    return d->session
        ? d->session->connectedServerVersion()
        : nx::utils::SoftwareVersion();
}

nx::Uuid SessionManager::localSystemId() const
{
    return d->session
        ? d->session->localSystemId()
        : nx::Uuid{};
}

QString SessionManager::currentUser() const
{
    return d->session
        ? d->session->currentUser()
        : QString{};
}

bool SessionManager::isCloudSession() const
{
    return d->session && d->session->isCloudSession();
}

} // namespace nx::vms::client::mobile
