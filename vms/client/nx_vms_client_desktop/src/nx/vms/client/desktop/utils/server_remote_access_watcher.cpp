// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "server_remote_access_watcher.h"

#include <unordered_map>

#include <core/resource/resource_property_key.h>
#include <core/resource_management/resource_pool.h>
#include <nx/cloud/vms_gateway/vms_gateway_embeddable.h>
#include <nx/network/http/http_types.h>
#include <nx/utils/uuid.h>
#include <nx/vms/api/data/port_forwarding_configuration.h>
#include <nx/vms/client/core/network/certificate_verifier.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/resource/server.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/system_logon/logic/remote_session.h>

namespace nx::vms::client::desktop {

ServerRemoteAccessWatcher::ServerRemoteAccessWatcher(
    SystemContext* systemContext,
    QObject* parent)
    :
    QObject(parent),
    SystemContextAware(systemContext)
{
    const auto resourcePool = this->resourcePool();

    connect(resourcePool,
        &QnResourcePool::resourcesAdded,
        this,
        &ServerRemoteAccessWatcher::onResourcesAdded);

    connect(resourcePool,
        &QnResourcePool::resourcesRemoved,
        this,
        &ServerRemoteAccessWatcher::onResourcesRemoved);

    if (auto session = this->systemContext()->session())
    {
        connect(session.get(),
            &nx::vms::client::desktop::RemoteSession::credentialsChanged,
            this,
            [this, resourcePool]
            {
                for (auto& resource: resourcePool->servers())
                {
                    const auto server = resource.objectCast<ServerResource>();
                    if (!NX_ASSERT(server))
                        continue;

                    updateRemoteAccess(server);
                }
            });
    }

    onResourcesAdded(resourcePool->servers());
}

ServerRemoteAccessWatcher::~ServerRemoteAccessWatcher()
{
}

void ServerRemoteAccessWatcher::onResourcesAdded(const QnResourceList& resources)
{
    for (const auto& resource: resources)
    {
        const auto server = resource.objectCast<ServerResource>();
        if (!server)
            continue;

        connect(server.get(),
            &QnResource::propertyChanged,
            this,
            [this, server](const QnResourcePtr& /*resource*/, const QString& key)
            {
                if (key == ResourcePropertyKey::Server::kPortForwardingConfigurations)
                    updateRemoteAccess(server);
            });

        updateRemoteAccess(server);
    }
}

void ServerRemoteAccessWatcher::onResourcesRemoved(const QnResourceList& resources)
{
    for (const auto& resource: resources)
    {
        const auto server = resource.objectCast<QnMediaServerResource>();
        if (!server)
            continue;

        const auto gateway = appContext()->cloudGateway();
        gateway->forward(server->getPrimaryAddress(), {}, nx::network::ssl::kAcceptAnyCertificate);

        server->disconnect(this);
    }
}

void ServerRemoteAccessWatcher::updateRemoteAccess(const ServerResourcePtr& server)
{
    using namespace nx::network::http::header;
    const auto gateway = appContext()->cloudGateway();
    auto configurations = server->portForwardingConfigurations();
    std::set<uint16_t> targetPorts;
    for (auto& configuration: configurations)
        targetPorts.insert(configuration.port);

    BearerAuthorization header(systemContext()->connectionCredentials().authToken.value);
    gateway->enforceHeadersFor(
        server->getPrimaryAddress(), {{kProxyAuthorization, header.toString()}});

    auto forwardedPorts = gateway->forward(server->getPrimaryAddress(),
        targetPorts,
        systemContext()->certificateVerifier()->makeAdapterFunc(
            server->getId(), server->getApiUrl()));

    std::vector<ForwardedPortConfiguration> fowardedPortConfiguration(
        configurations.begin(), configurations.end());

    for (auto& configuration: fowardedPortConfiguration)
        configuration.forwardedPort = forwardedPorts[configuration.port];

    server->setForwardedPortConfigurations(fowardedPortConfiguration);
}

} // namespace nx::vms::client::desktop
