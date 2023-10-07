// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/utils/impl_ptr.h>
#include <nx/vms/api/data/module_information.h>
#include <nx/vms/common/system_context.h>
#include <nx/vms/utils/abstract_session_token_helper.h>

#include "system_context_aware.h" //< Forward declarations.

class QnPtzControllerPool;
class QQmlContext;

namespace ec2 {

class AbstractECConnection;
using AbstractECConnectionPtr = std::shared_ptr<AbstractECConnection>;

} // namespace ec2

namespace nx::network::http { class Credentials; }
namespace nx::vms::api { struct SystemSettings; }
namespace nx::vms::rules { class Engine; }

namespace nx::vms::client::core {

class AccessController;
class UserWatcher;
class WatermarkWatcher;

class NX_VMS_CLIENT_CORE_API SystemContext: public common::SystemContext
{
    Q_OBJECT
    using base_type = common::SystemContext;

public:
    /**
     * Initialize client-core-level System Context based on existing common-level System Context.
     * Destruction order must be handled by the caller.
     * @param peerId Id of the current peer in the Message Bus. It is persistent and is not changed
     *     between the application runs. Desktop Client calculates actual peer id depending on the
     *     stored persistent id and on the number of the running client instance, so different
     *     Client windows have different peer ids.
     * @param resourceAccessMode Mode of the Resource permissions mechanism work. Direct mode is
     *     used on the Server side, all calculations occur on the fly. Cached mode is used for the
     *     current context on the Client side, where we need to actively listen for the changes and
     *     emit signals. Cross-system contexts also use direct mode.
     */
    SystemContext(
        Mode mode,
        QnUuid peerId,
        nx::core::access::Mode resourceAccessMode = nx::core::access::Mode::cached,
        QObject* parent = nullptr);
    virtual ~SystemContext() override;

    static SystemContext* fromResource(const QnResourcePtr& resource);

    static std::unique_ptr<nx::vms::common::SystemContextInitializer> fromQmlContext(
        QObject* source);
    void storeToQmlContext(QQmlContext* qmlContext);

    /**
     * Information about the Server we are connected to.
     */
    nx::vms::api::ModuleInformation moduleInformation() const;

    /**
     * Update remote session this Context belongs to. Current client architecture supposes one main
     * System Context to exist through whole Application lifetime, and different Sessions are
     * loaded into it and unloaded when session is terminated.
     * // TODO: #sivanov Invert architecture, so Remote Session will own the System Context.
     */
    void setSession(std::shared_ptr<RemoteSession> session);

    /**
     * Set connection which this Context should use to communicate with the corresponding System.
     * Connection is mutually exclusive with ::setSession() and should be used for session-less
     * Contexts only.
     */
    void setConnection(RemoteConnectionPtr connection);

    /**
     * Id of the server which was used to establish the Remote Session (if it is present).
     */
    QnUuid currentServerId() const;

    /**
     * The server which was used to establish the Remote Session (if it is present).
     */
    QnMediaServerResourcePtr currentServer() const;

    /**
     * Remote session this context belongs to (if any).
     */
    std::shared_ptr<RemoteSession> session() const;

    /**
     * Connection which this Context should use to communicate with the corresponding System.
     */
    RemoteConnectionPtr connection() const;

    /**
     * Credentials we are using to authorize the connection.
     */
    nx::network::http::Credentials connectionCredentials() const;

    /** API interface of the currently connected server. */
    rest::ServerConnectionPtr connectedServerApi() const;

    /**
     * Established p2p connection (if any).
     */
    ec2::AbstractECConnectionPtr messageBusConnection() const;

    QnPtzControllerPool* ptzControllerPool() const;

    UserWatcher* userWatcher() const;

    WatermarkWatcher* watermarkWatcher() const;

    ServerTimeWatcher* serverTimeWatcher() const;

    virtual nx::vms::common::SessionTokenHelperPtr getSessionTokenHelper() const;

    AccessController* accessController() const;

signals:
    void remoteIdChanged(const QnUuid& id);

protected:
    virtual void setMessageProcessor(QnCommonMessageProcessor* messageProcessor) override;
    void resetAccessController(AccessController* accessController);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
