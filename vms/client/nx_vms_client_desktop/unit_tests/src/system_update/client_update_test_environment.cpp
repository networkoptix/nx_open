// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "client_update_test_environment.h"

#include <client/client_runtime_settings.h>
#include <client/client_startup_parameters.h>
#include <client_core/client_core_module.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/resource/server.h>
#include <ui/workbench/workbench_context.h>

namespace os {

const nx::utils::OsInfo ubuntu("linux_x64", "ubuntu");
const nx::utils::OsInfo ubuntu14("linux_x64", "ubuntu", "14.04");
const nx::utils::OsInfo ubuntu16("linux_x64", "ubuntu", "16.04");
const nx::utils::OsInfo ubuntu18("linux_x64", "ubuntu", "18.04");
const nx::utils::OsInfo windows("windows_x64");

} // namespace os

namespace nx::vms::client::desktop::system_update::test {

ClientVerificationData ClientUpdateTestEnvironment::makeClientData(nx::utils::SoftwareVersion version)
{
    ClientVerificationData data;
    data.osInfo = os::windows;
    data.currentVersion = version;
    data.clientId = QnUuid("cccccccc-cccc-cccc-cccc-cccccccccccc");
    return data;
}

QnMediaServerResourcePtr ClientUpdateTestEnvironment::makeServer(
    nx::utils::SoftwareVersion version,
    bool online,
    bool compatibleServer)
{
    QnMediaServerResourcePtr server(new nx::vms::client::desktop::ServerResource());
    server->setVersion(version);
    server->setIdUnsafe(QnUuid::createUuid());
    server->setOsInfo(os::windows);
    resourcePool()->addResource(
        server,
        compatibleServer
            ? QnResourcePool::NoAddResourceFlags
            : QnResourcePool::UseIncompatibleServerPool);

    server->setStatus(online
        ? nx::vms::api::ResourceStatus::online
        : nx::vms::api::ResourceStatus::offline);
    return server;
}

std::map<QnUuid, QnMediaServerResourcePtr> ClientUpdateTestEnvironment::getAllServers()
{
    std::map<QnUuid, QnMediaServerResourcePtr> result;
    for (auto server: resourcePool()->servers())
        result[server->getId()] = server;
    return result;
}

void ClientUpdateTestEnvironment::removeAllServers()
{
    auto servers = resourcePool()->servers();
    resourcePool()->removeResources(servers);
}

QnResourcePool* ClientUpdateTestEnvironment::resourcePool() const
{
    return systemContext()->resourcePool();
}

} // namespace nx::vms::client::desktop::system_update::test
