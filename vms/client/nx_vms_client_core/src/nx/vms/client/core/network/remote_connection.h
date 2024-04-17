// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <QtCore/QObject>

#include <api/server_rest_connection_fwd.h>
#include <nx/utils/impl_ptr.h>
#include <nx/utils/serialization/format.h>
#include <nx/vms/api/data/login.h>
#include <nx/vms/api/data/module_information.h>
#include <nx/vms/api/types/connection_types.h>
#include <nx/vms/time/abstract_time_sync_manager.h>
#include <nx_ec/ec_api_fwd.h>

#include "connection_info.h"
#include "logon_data.h"

class QnCommonModule;

namespace rest { class ServerConnection; }

namespace nx::vms::client::core {

class CertificateCache;
class CertificateVerifier;

/**
 * Actual system connection, provides access to different manager classes, which in their turn
 * represent an abstraction layer to send and receive transactions. Managers are using query
 * processor adaptor via this class interface to send actual network requests.
 *
 * Provides unified access to the Server REST API.
 *
 * Stores actual connection info and allows to modify it e.g. when we are changing current user
 * password or find a quicker network interface.
 */
class NX_VMS_CLIENT_CORE_API RemoteConnection: public QObject
{
    Q_OBJECT

public:
    static const nx::utils::SoftwareVersion kRestApiSupportVersion;

    RemoteConnection(
        nx::vms::api::PeerType peerType,
        const nx::vms::api::ModuleInformation& moduleInformation,
        ConnectionInfo connectionInfo,
        std::optional<std::chrono::microseconds> sessionTokenExpirationTime,
        std::shared_ptr<CertificateCache> certificateCache,
        Qn::SerializationFormat serializationFormat,
        QnUuid sessionId,
        QObject* parent = nullptr);
    virtual ~RemoteConnection() override;

    /** Initialize message bus connection. Actual for the current connection only. */
    void initializeMessageBusConnection(QnCommonModule* commonModule);

    const nx::vms::api::ModuleInformation& moduleInformation() const;

    /** Address and credentials of the server we are currently connected to. */
    ConnectionInfo connectionInfo() const;

    /** Data to login to this System. */
    LogonData createLogonData() const;

    /** Address of the server we are currently connected to. */
    nx::network::SocketAddress address() const;

    /**
     * Changes address which client currently uses to connect to server. Required to handle
     * situations like server port change or systems merge. Also can be used to select the most
     * fast interface amongst available.
     */
    void updateAddress(nx::network::SocketAddress address);

    nx::vms::api::UserType userType() const;

    /** Credentials we are using to authorize the connection. */
    nx::network::http::Credentials credentials() const;

    /**
     * Changes credentials which client currently uses to connect to server. Required to handle
     * situations like user password change or systems merge.
     */
    void updateCredentials(
        nx::network::http::Credentials credentials,
        std::optional<std::chrono::microseconds> sessionTokenExpirationTime = std::nullopt);

    std::optional<std::chrono::microseconds> sessionTokenExpirationTime() const;
    void setSessionTokenExpirationTime(std::optional<std::chrono::microseconds> time);

    rest::ServerConnectionPtr serverApi() const;

    ec2::AbstractECConnectionPtr messageBusConnection() const;
    common::AbstractTimeSyncManagerPtr timeSynchronizationManager() const;

    void updateSessionId(const QnUuid& sessionId);

    std::shared_ptr<CertificateCache> certificateCache() const;

    bool isRestApiSupported() const;

signals:
    void credentialsChanged();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

using RemoteConnectionPtr = std::shared_ptr<RemoteConnection>;

} // namespace nx::vms::client::core
