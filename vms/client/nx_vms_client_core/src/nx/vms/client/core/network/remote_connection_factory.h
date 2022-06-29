// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <functional>
#include <optional>
#include <variant>

#include <nx/fusion/serialization_format.h>
#include <nx/utils/impl_ptr.h>
#include <nx/vms/api/types/connection_types.h>
#include <nx_ec/abstract_ec_connection_factory.h>
#include <nx_ec/ec_api_common.h>
#include <nx_ec/ec_api_fwd.h>

#include "connection_info.h"
#include "remote_connection_error.h"
#include "remote_connection_factory_context.h"
#include "remote_connection_fwd.h"

class QObject;
class QnCommonModule;
struct QnPingReply;

namespace nx::vms::client::core {

class CertificateVerifier;

/**
 * Central storage class for all transactions-related classes. Owns message bus, query processor,
 * (de)serializers, etc. Creates instanses of RemoteConnection and provides an access to the
 * mentioned classes.
 */
class RemoteConnectionFactory: public ec2::AbstractECConnectionFactory
{
public:
    using Context = RemoteConnectionFactoryContext;
    using ContextPtr = std::shared_ptr<Context>;

    using Process = RemoteConnectionProcess;
    using ProcessPtr = std::shared_ptr<Process>;

    /**
     * Common module and certificate validator ownership is not taken. Their lifetime must be
     * controlled by the caller.
     */
    RemoteConnectionFactory(
        QnCommonModule* commonModule,
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

    using ConnectionOrError = std::variant<RemoteConnectionPtr, RemoteConnectionError>;
    using Callback = std::function<void(ConnectionOrError)>;

    /**
     * Create connection to the target VMS server.
     * @param connectionInfo Server endpoint address and authentication information.
     * @param expectedServerId Id of the server we expect to connect.
     * @param callback Function which is called when connect process is finished. Accepts connection
     *     object or error code as a single parameter.
     * @return context object, which guards the process. If it is destroyed, no callback will be
     *     called.
     */
    [[nodiscard]]
    ProcessPtr connect(
        ConnectionInfo connectionInfo,
        std::optional<QnUuid> expectedServerId,
        Callback callback,
        Context::Purpose purpose = Context::Purpose::connect,
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
