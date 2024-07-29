// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "server_remote_access_watcher.h"

#include <unordered_map>

#include <core/resource/resource_property_key.h>
#include <core/resource_management/resource_pool.h>
#include <nx/cloud/vms_gateway/vms_gateway_embeddable.h>
#include <nx/network/http/http_types.h>
#include <nx/utils/uuid.h>
#include <nx/vms/api/data/port_forwarding_configuration.h>
#include <nx/vms/api/types/proxy_connection_access_policy.h>
#include <nx/vms/client/core/access/access_controller.h>
#include <nx/vms/client/core/network/certificate_verifier.h>
#include <nx/vms/client/core/network/remote_connection.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/resource/server.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/system_logon/logic/remote_session.h>
#include <nx/vms/common/system_settings.h>

namespace {

static const QString kProxyConnectionAccessPolicy = "proxyConnectionAccessPolicy";

} // namespace

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

    connect(systemSettings(),
        &nx::vms::common::SystemSettings::ec2ConnectionSettingsChanged,
        this,
        [this](const QString& key)
        {
            if (key == kProxyConnectionAccessPolicy)
                updateRemoteAccessForAllServers();
        });

    connect(accessController(),
        &nx::vms::client::core::AccessController::globalPermissionsChanged,
        this,
        &ServerRemoteAccessWatcher::updateRemoteAccessForAllServers);

    if (auto session = this->systemContext()->session())
    {
        connect(session.get(),
            &nx::vms::client::desktop::RemoteSession::credentialsChanged,
            this,
            &ServerRemoteAccessWatcher::updateRemoteAccessForAllServers);
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
        gateway->forward(server->getPrimaryAddress(),
            /*targetPorts*/ {},
            nx::network::ssl::kAcceptAnyCertificate);

        server->disconnect(this);
    }
}

void ServerRemoteAccessWatcher::updateRemoteAccess(const ServerResourcePtr& server)
{
    using namespace nx::network::http::header;
    const auto gateway = appContext()->cloudGateway();
    auto address = systemContext()->connection()->connectionInfo().isCloud()
        ? server->getCloudAddress().value()
        : server->getPrimaryAddress();
    RemoteAccessData remoteAccessData;
    remoteAccessData.enabledForCurrentUser = isRemoteAccessEnabledForCurrentUser();
    if (!remoteAccessData.enabledForCurrentUser)
    {
        gateway->forward(address,
            /*targetPorts*/ {},
            nx::network::ssl::kAcceptAnyCertificate);
        server->setRemoteAccessData(remoteAccessData);
        return;
    }
    auto configurations = server->portForwardingConfigurations();
    std::set<uint16_t> targetPorts;
    for (auto& configuration: configurations)
        targetPorts.insert(configuration.port);

    BearerAuthorization header(systemContext()->connectionCredentials().authToken.value);
    gateway->enforceHeadersFor(address, {{kProxyAuthorization, header.toString()}});

    auto forwardedPorts = gateway->forward(address,
        targetPorts,
        systemContext()->certificateVerifier()->makeAdapterFunc(
            server->getId(), server->getApiUrl()));

    std::vector<ForwardedPortConfiguration> fowardedPortConfiguration(
        configurations.begin(), configurations.end());

    for (auto& configuration: fowardedPortConfiguration)
    {
        if (auto itr = forwardedPorts.find(configuration.port); itr != forwardedPorts.end())
            configuration.forwardedPort = itr->second;
        else
            NX_WARNING(this, "Failed to forward port %1", configuration.port);
    }

    remoteAccessData.forwardedPortConfigurations = fowardedPortConfiguration;

    server->setRemoteAccessData(remoteAccessData);
}

void ServerRemoteAccessWatcher::updateRemoteAccessForAllServers()
{
    for (auto& resource: resourcePool()->servers())
    {
        const auto server = resource.objectCast<ServerResource>();
        if (!NX_ASSERT(server))
            continue;

        updateRemoteAccess(server);
    }
}

bool ServerRemoteAccessWatcher::isRemoteAccessEnabledForCurrentUser()
{
    using namespace nx::vms::api;

    switch (systemSettings()->proxyConnectionAccessPolicy())
    {
        case ProxyConnectionAccessPolicy::disabled:
            return false;
        case ProxyConnectionAccessPolicy::powerUsers:
            return accessController()->hasPowerUserPermissions();
        case ProxyConnectionAccessPolicy::admins:
        {
            return accessController()->globalPermissions().testFlag(
                GlobalPermission::administrator);
        }
    }

    return false;
}

} // namespace nx::vms::client::desktop
