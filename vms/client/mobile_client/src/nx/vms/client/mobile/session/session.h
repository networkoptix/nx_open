// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <client/client_connection_status.h>
#include <nx/network/http/auth_tools.h>
#include <nx/network/socket_common.h>
#include <nx/utils/impl_ptr.h>
#include <nx/utils/software_version.h>
#include <nx/utils/uuid.h>
#include <nx/vms/client/core/network/remote_connection_error.h>
#include <nx/vms/client/core/system_context_aware.h>

#include "session_types.h"

namespace nx::vms::client::mobile {

/**
 * Represents single connection session to some system. Automatically tries to restore connection
 * in case of loss. If first connection fails, it is supposed that session failed and should be
 * reset by session manager. Only one connection is supposed to exist at each moment of time.
 */
class Session: public QObject, public nx::vms::client::core::SystemContextAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    enum class ConnectionState
    {
        /**
         * Initial state for the session. Should not be used after session was created
         * and until it ends.
         */
        disconnected = static_cast<int>(QnConnectionState::Disconnected),

        /**
         * First step in connection process - tries to check physical connection
         * and evaluate target server parameters such a version, customization, etc.
         * Can't be used after initial connection process is done.
         */
        connecting = static_cast<int>(QnConnectionState::Connecting),

        /**
         * All checks are passed successfully. Waiting for resources from the server.
         * Can't be used after initial connection process is done.
         */
        loading = static_cast<int>(QnConnectionState::Connected),

        /** Occures only if initial connection process was successful.*/
        reconnecting = static_cast<int>(QnConnectionState::Reconnecting),

        /** Connection was successful, all needed information was received from the server. */
        ready = static_cast<int>(QnConnectionState::Ready),

        /** Application was suspended. */
        suspended
    };

private:
    /**
     * Currently used address. Could be changed when reconnecting to the another server of the system.
     */
    Q_PROPERTY(nx::network::SocketAddress address
        READ address
        NOTIFY addressChanged)

    /** Current connection state. */
    Q_PROPERTY(ConnectionState connectionState
        READ connectionState
        NOTIFY connectionStateChanged)

    /** Shows if session currently tries to restore connection. */
    Q_PROPERTY(bool restoringConnection
        READ restoringConnection
        NOTIFY restoringConnectionChanged)
    /** Shows if session has been connected at least once before. */
    Q_PROPERTY(bool wasConnected
        READ wasConnected
        NOTIFY wasConnectedChanged)

    /** Shows if session use cloud connection. */
    Q_PROPERTY(bool isCloudSession
        READ isCloudSession
        NOTIFY isCloudSessionChanged)

    /** Reresents system name of the session. */
    Q_PROPERTY(QString systemName
        READ systemName
        NOTIFY systemNameChanged)

    /** Version of the server to which we are connected to. */
    Q_PROPERTY(nx::utils::SoftwareVersion connectedServerVersion
        READ connectedServerVersion
        NOTIFY connectedServerVersionChanged)

public:
    using Holder = std::shared_ptr<Session>;

    struct LocalConnectionData
    {
        nx::utils::Url url;
        nx::network::http::Credentials credentials;
    };

    struct CloudConnectionData
    {
        QString cloudSystemId;
        std::optional<nx::network::http::Credentials> digestCredentials;
    };

    using ConnectionData = std::variant<LocalConnectionData, CloudConnectionData>;
    /**
     * Creates new session using specified connection info.
     * @param connectionData Data for connection.
     * @param supposedSystemName Represents supposed system name which in some cases we are able
     * to predict before connection is done.
     * @param callback Callback to signalize if initial connection is successfull or not.
     * @param parent Object parent. Used to initialize common module aware stuff.
     */
    static Holder create(
        core::SystemContext* systemContext,
        const ConnectionData& connectionData,
        const QString& supposedSystemName,
        session::ConnectionCallback&& callback);

    virtual ~Session() override;

public:
    // Properties section.
    nx::network::SocketAddress address() const;
    ConnectionState connectionState() const;
    bool restoringConnection() const;
    bool wasConnected() const;
    bool isCloudSession() const;
    QString systemName() const;
    ConnectionData connectionData() const;
    nx::utils::SoftwareVersion connectedServerVersion() const;

    static constexpr std::string_view kCloudUrlScheme = "cloud";
    nx::Uuid localSystemId() const;
    QString currentUser() const;

    void setSuspended(bool suspended);

signals:
    void isCloudSessionChanged();
    void addressChanged();
    void connectionStateChanged();
    void restoringConnectionChanged();
    void wasConnectedChanged();
    void systemNameChanged();
    void connectedServerVersionChanged();

    /** Signalizes that session successfully restored after connection loss. */
    void restored();

    /**
     * Signalizes when session is stopped due to error in initial connection process
     * or due to unavailability to restore.
     */
    void finishedWithError(core::RemoteConnectionErrorCode errorStatus);

    /** Signalizes that session parameters changed. */
    void parametersChanged(
        const nx::Uuid& localSystemId,
        const QString& user);

private:
    /** Constructor. Creates and initializes session. Parameters are equal to "create" function. */
    Session(
        core::SystemContext* systemContext,
        const ConnectionData& connectionData,
        const QString& supposedSystemName,
        session::ConnectionCallback&& callback,
        QObject* parent = nullptr);

private:
    class Private;
    nx::utils::ImplPtr<Private> d;
};

QString toString(Session::ConnectionState state);

} // namespace nx::vms::client::mobile
