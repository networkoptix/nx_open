// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/utils/impl_ptr.h>
#include <nx/vms/common/system_context.h>

#include "system_context_aware.h" //< Forward declarations.

class QQmlContext;

namespace nx::vms::client::core {

class SystemContext: public nx::vms::common::SystemContext
{
    Q_OBJECT

public:
    /**
     * Initialize client-core-level System Context based on existing common-level System Context.
     * Destruction order must be handled by the caller.
     * @param peerId Id of the current peer in the Message Bus. It is persistent and is not changed
     *     between the application runs. Desktop Client calculates actual peer id depending on the
     *     stored persistent id and on the number of the running client instance, so different
     *     Client windows have different peer ids.
     */
    SystemContext(QnUuid peerId, QObject* parent = nullptr);
    virtual ~SystemContext() override;

    static std::unique_ptr<nx::vms::common::SystemContextInitializer> fromQmlContext(
        QObject* source);
    void storeToQmlContext(QQmlContext* qmlContext);

    /**
     * Actual remote session this Context belongs to. See ::setSession for the details.
     */
    std::shared_ptr<RemoteSession> session() const;

    /**
     * Update remote session this Context belongs to. Current client architecture supposes one main
     * System Context to exist through whole Application lifetime, and different Sessions are
     * loaded into it and unloaded when session is terminated.
     * // TODO: #sivanov Invert architecture, so Remote Session will own the System Context.
     */
    void setSession(std::shared_ptr<RemoteSession> session);

    /**
     * Id of the server which was used to establish the Remote Session (if it is present).
     */
    QnUuid currentServerId() const;

    /**
     * The server which was used to establish the Remote Session (if it is present).
     */
    QnMediaServerResourcePtr currentServer() const;

    RemoteConnectionPtr connection() const;

    //ec2::AbstractECConnectionPtr messageBusConnection() const;

    ///** Address of the server we are currently connected to. */
    //nx::network::SocketAddress connectionAddress() const;

    ///** Credentials we are using to authorize the connection. */
    //nx::network::http::Credentials connectionCredentials() const;

    /** API interface of the currently connected server. */
    rest::ServerConnectionPtr connectedServerApi() const;

    ServerTimeWatcher* serverTimeWatcher() const;

signals:
    void remoteIdChanged(const QnUuid& id);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
