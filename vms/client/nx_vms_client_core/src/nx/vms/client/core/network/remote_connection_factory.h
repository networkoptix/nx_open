// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <functional>
#include <optional>
#include <variant>

#include <nx/utils/impl_ptr.h>
#include <nx/utils/serialization/format.h>
#include <nx/vms/api/types/connection_types.h>
#include <nx_ec/abstract_ec_connection_factory.h>
#include <nx_ec/ec_api_common.h>
#include <nx_ec/ec_api_fwd.h>

#include "logon_data.h"
#include "remote_connection_error.h"
#include "remote_connection_factory_context.h"
#include "remote_connection_fwd.h"

class QObject;
struct QnPingReply;

namespace nx::vms::client::core {

class AbstractRemoteConnectionFactoryRequestsManager;
class CertificateVerifier;

/**
 * Central storage class for all transactions-related classes. Owns message bus, query processor,
 * (de)serializers, etc. Creates instanses of RemoteConnection and provides an access to the
 * mentioned classes.
 */
class NX_VMS_CLIENT_CORE_API RemoteConnectionFactory: public ec2::AbstractECConnectionFactory
{
public:
    using Context = RemoteConnectionFactoryContext;
    using ContextPtr = std::shared_ptr<Context>;

    using Process = RemoteConnectionProcess;
    using ProcessPtr = std::shared_ptr<Process>;

    using AuditIdProvider = std::function<nx::Uuid()>;

    struct CloudCredentialsProvider
    {
        std::function<nx::network::http::Credentials()> getCredentials;
        std::function<std::string()> getLogin;
        std::function<std::string()> getDigestPassword;
        std::function<bool()> is2FaEnabledForUser;
    };

    /**
     * Certificate validator ownership is not taken. Its lifetime must be controlled by the caller.
     */
    RemoteConnectionFactory(
        AuditIdProvider auditIdProvider,
        CloudCredentialsProvider cloudCredentialsProvider,
        std::shared_ptr<AbstractRemoteConnectionFactoryRequestsManager> requestsManager,
        CertificateVerifier* certificateVerifier,
        nx::vms::api::PeerType peerType,
        Qn::SerializationFormat serializationFormat);

    virtual ~RemoteConnectionFactory() override;

    /**
     * Some login scenarios require user interaction. E.g. client-side certificate verifier requires
     * UI to suggest user accept or reject suspicious TLS certificate, etc.
     */
    void setUserInteractionDelegate(
        std::unique_ptr<AbstractRemoteConnectionUserInteractionDelegate> delegate);

    /**
     * Access to user interaction delegate to re-use it in the separate server cetificate watcher
     * class. When server certificate changes, we should display totally the same dialogs as when
     * connecting to the system.
     */
    AbstractRemoteConnectionUserInteractionDelegate* userInteractionDelegate() const;

    using ConnectionOrError = std::variant<RemoteConnectionPtr, RemoteConnectionError>;
    using Callback = std::function<void(ConnectionOrError)>;

    /**
     * Create connection to the target VMS server.
     * @param logonData Server endpoint address, authentication information and other logon data.
     * @param callback Function which is called when connect process is finished. Accepts
     *     connection object or error code as a single parameter.
     * @return context object, which guards the process. If it is destroyed, no callback will be
     *     called.
     */
    [[nodiscard]]
    ProcessPtr connect(
        LogonData logonData,
        Callback callback,
        std::unique_ptr<AbstractRemoteConnectionUserInteractionDelegate>
            customUserInteractionDelegate = {});

    virtual void shutdown() override;

    /**
     * Destroys a connection process asynchronously.
     */
    static void destroyAsync(ProcessPtr&& process);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::core
