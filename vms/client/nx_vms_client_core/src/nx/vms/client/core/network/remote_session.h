// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <nx/network/http/auth_tools.h>
#include <nx/network/socket_common.h>
#include <nx/utils/impl_ptr.h>
#include <nx/utils/uuid.h>
#include <nx/vms/client/core/system_context_aware.h>

#include "remote_connection_fwd.h"

namespace nx::vms::client::core {

/**
 * Logically established connection to the System. Corresponds to the system UI: client looks like
 * logged in.
 * Class responsibilities:
 * * Stores and maintains actual RemoteConnection instance. Creates a new one if needed in case of
 *   reconnect or user authentication type change (// TODO: #esobolev).
 * * Implements auto-reconnect feature in case currently connected Server went offline.
 * * Watches actual cloud session and breaks current System connection in case user is logged out of
 *   cloud, being logged using cloud credentials (// TODO: #amalov).
 */
class NX_VMS_CLIENT_CORE_API RemoteSession:
    public QObject,
    public SystemContextAware
{
    using base_type = QObject;
    Q_OBJECT

public:
    /** Session state. What is actually is going on. */
    NX_REFLECTION_ENUM_CLASS_IN_CLASS(State,
        /** Select the server address, waiting for connection. */
        reconnecting,

        /** Connection established, waiting for peerFound. */
        waitingPeer,

        /** Peer found, waiting for resources. */
        waitingResources,

        /** Connected and ready to work. */
        connected)

    RemoteSession(
        RemoteConnectionPtr connection,
        SystemContext* systemContext,
        QObject* parent = nullptr);
    virtual ~RemoteSession() override;

    void updatePassword(const QString& newPassword);
    void updateBearerToken(std::string token);
    void updateCloudSessionToken();

    /** Allow to reconnect only to the current server (disabled by default). */
    void setStickyReconnect(bool value);

    State state() const;

    /**
     * Interface to the server we are currently connected to.
     *
     * Note: connection always exists, function never returns null pointer.
     */
    RemoteConnectionPtr connection() const;

    /** Auto-terminate server session when destroying. */
    void setAutoTerminate(bool value);

    std::chrono::microseconds age() const;

signals:
    void stateChanged(State state);
    void reconnectingToServer(const QnMediaServerResourcePtr& server);

    /**
     * Notify that reconnect was not successful. Returned error code, corresponding to the current
     * server failure.
     */
    void reconnectFailed(RemoteConnectionErrorCode errorCode);

    void reconnectRequired();

    void credentialsChanged();

    void tokenExpirationTimeChanged();

protected:
    virtual bool keepCurrentServerOnError(RemoteConnectionErrorCode errorCode);

private:
    /**
     * Mark selected connection as current. Here is the main storage of the connection, so
     * connection instance is destroyed when cleared here.
     */
    void establishConnection(RemoteConnectionPtr connection);

    void setState(State value);

    void onMessageBusConnectionOpened();
    void onInitialResourcesReceived();
    void onMessageBusConnectionClosed();
    void onCloudSessionTokenExpiring();

    void tryToRestoreConnection();
    void stopReconnecting();
    void reconnectStep();

    /** Check if reconnect failed and send corresponding signal. */
    bool checkIfReconnectFailed();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::core
