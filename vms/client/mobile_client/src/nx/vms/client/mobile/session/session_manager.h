// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtQml/QJSValue>

#include <nx/network/http/auth_tools.h>
#include <nx/utils/impl_ptr.h>
#include <nx/utils/software_version.h>
#include <nx/utils/url.h>
#include <nx/utils/uuid.h>
#include <nx/vms/client/core/network/remote_connection_error.h>
#include <nx/vms/client/mobile/system_context_aware.h>

#include "session.h"

namespace nx::vms::client::mobile {

class SystemContext;

/**
 * Responsible for connection sessions management. Only one session should exist at each moment of
 * time. Resets current session if initial connection is failed or some fatal error is occured.
 */
class SessionManager:
    public QObject,
    public SystemContextAware
{
    Q_OBJECT
    using base_type = QObject;

    /** Shows if session exists. */
    Q_PROPERTY(bool hasSession
        READ hasSession
        NOTIFY hasSessionChanged)

    /** Shows if session exists and initial connection is in progress. */
    Q_PROPERTY(bool hasConnectingSession
        READ hasConnectingSession
        NOTIFY stateChanged)

    /**
     * Shows if session exists, physically connected to the server and awaiting for resources now.
     */
    Q_PROPERTY(bool hasAwaitingResourcesSession
        READ hasAwaitingResourcesSession
        NOTIFY stateChanged)

    /** Shows if session exists and connection to server has already established at least once. */
    Q_PROPERTY(bool hasActiveSession
        READ hasActiveSession
        NOTIFY stateChanged)

    /** Shows if session exists and connection to the server is established now. */
    Q_PROPERTY(bool hasConnectedSession
        READ hasConnectedSession
        NOTIFY hasConnectedSessionChanged)

    /**
     * Shows if sessions exists, connection has established at least once and now reconnecting
     * to the server.
     */
    Q_PROPERTY(bool hasReconnectingSession
        READ hasReconnectingSession
        NOTIFY hasReconnectingSessionChanged)

    /** Represents current session host. */
    Q_PROPERTY(QString sessionHost
        READ sessionHost
        NOTIFY sessionHostChanged)

    /** Shows if current session use cloud connection. */
    Q_PROPERTY(bool isCloudSession
        READ isCloudSession
        NOTIFY sessionHostChanged)

    /** Reresents system name of the current session. */
    Q_PROPERTY(QString systemName
        READ systemName
        NOTIFY systemNameChanged)

    /** Version of the server for the current session. */
    Q_PROPERTY(nx::utils::SoftwareVersion connectedServerVersion
        READ connectedServerVersion
        NOTIFY connectedServerVersionChanged)

public:
    enum ConnectionStatus
    {
        SuccessConnectionStatus,
        UserTemporaryLockedOutConnectionStatus,
        UnauthorizedConnectionStatus,
        IncompatibleVersionConnectionStatus,
        LocalSessionExiredStatus,
        OtherErrorConnectionStatus
    };
    Q_ENUM(ConnectionStatus)

    static void registerQmlType();

    SessionManager(SystemContext* context, QObject* parent = nullptr);
    virtual ~SessionManager() override;

    /**
     * Start new session with the specified url and credentials.
     * @param url Url of the target server.
     * @param credentials Credentials to be used for the connection.
     */
    void startSessionWithCredentials(
        const nx::utils::Url& url,
        const nx::network::http::Credentials& credentials,
        session::ConnectionCallback&& callback = session::ConnectionCallback());

    /**
     * Start new session with connection to the specified cloud system. May be called from the
     * QML code.
     * @param cloudSystemId Identifier of the target system.
     * @param supposedSystemName Represents supposed system name which in some cases we are able
     * to predict before connection is done.
     */
    Q_INVOKABLE void startCloudSession(
        const QString& cloudSystemId,
        const QString& supposedSystemName);

    /**
     * Start new session with connection to the specified old cloud system with the digest
     * authentication. May be called from the QML code.
     * @param cloudSystemId Identifier of the target system.
     * @param password Cloud password for the currently logged user.
     * @param supposedSystemName Represents supposed system name which in some cases we are able
     *     to predict before connection is done.
     */
    Q_INVOKABLE void startDigestCloudSession(
        const QString& cloudSystemId,
        const QString& password,
        const QString& supposedSystemName = {});

    /**
     * Start new session with connection to the specified cloud system. Callback with connection
     * result will be called (if specified)
     * @param cloudSystemId Identifier of the target system.
     * @param digestCredentials Optional digest credentials which may be used for the connection
     * to the old servers.
     * @param callback Callback which will be called with connection result.
     */
    void startCloudSession(
        const QString& cloudSystemId,
        const std::optional<nx::network::http::Credentials>& digestCredentials = {},
        session::ConnectionCallback&& callback = {});

    /**
     * Start new session and try to load stored credentials by the specified local system id and
     * user name.
     * @param url Url of the target server.
     * @param localSystemId Id of the local system to find corresponding credentials.
     * @param user User name to find corresponding credentials.
     * @return Whether the connection credentials were found and connection process was started.
     */
    Q_INVOKABLE bool startSessionWithStoredCredentials(
        const nx::utils::Url& url,
        const nx::Uuid& localSystemId,
        const QString& user,
        QJSValue callback = QJSValue());

    /**
     * Start new session using specified, user name and password.
     * @param url Url of the target server.
     * @param user User name to be used for the connection.
     * @param password Password to be used for the connection.
     * @param supposedSystemName Represents supposed system name which in some cases we are able
     * to predict before connection is done.
     * @param callback Callback to signalize to the QML part if initial connection
     * is successfull or not.
     */
    Q_INVOKABLE void startSession(
        const nx::utils::Url& url,
        const QString& user,
        const QString& password,
        const QString& supposedSystemName,
        QJSValue callback = QJSValue());

    /**
     * Start new session using specified url.
     * @param url Url of the target server. Should contain user name and password for authentication.
     * @param callback Callback to signalize if initial connection is successfull or not.
     */
    void startSessionByUrl(
        const nx::utils::Url& url,
        session::ConnectionCallback&& callback = session::ConnectionCallback());

    /** Stop currently running session (if any). */
    Q_INVOKABLE void stopSession();

public:
    // Properties section.
    bool hasSession() const;
    bool hasConnectingSession() const;
    bool hasAwaitingResourcesSession() const;
    bool hasActiveSession() const;
    bool hasConnectedSession() const;
    bool hasReconnectingSession() const;
    bool isCloudSession() const;
    QString sessionHost() const;
    QString systemName() const;
    nx::utils::SoftwareVersion connectedServerVersion() const;
    nx::Uuid localSystemId() const;
    QString currentUser() const;

    void setSuspended(bool suspended);

signals:
    void stateChanged();
    void hasSessionChanged();
    void hasConnectedSessionChanged();
    void hasReconnectingSessionChanged();
    void sessionHostChanged();
    void systemNameChanged();
    void connectedServerVersionChanged();

    /** Signalizes once if session started successfully. */
    void sessionStartedSuccessfully();

    /** Signalizes when session is stoped by the user (called stopSession). */
    void sessionStoppedManually();

    /** Signalizes that session successfully restored after connection loss. */
    void sessionRestored();

    /**
     * Signalizes when session is stopped due to error in initial connection process
     * or due to unavailability to restore.
     */
    void sessionFinishedWithError(
        const Session::Holder& session,
        nx::vms::client::core::RemoteConnectionErrorCode status);

    /** Signalizes that session parameters changed. */
    void sessionParametersChanged(
        const nx::Uuid& localSystemId,
        const QString& user);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::mobile
