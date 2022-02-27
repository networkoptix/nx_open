// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "client_update_test_environment.h"

#include <client/client_runtime_settings.h>
#include <client/client_startup_parameters.h>
#include <client_core/client_core_module.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_runtime_data.h>
#include <core/resource/media_server_resource.h>

#include <ui/workbench/workbench_context.h>

namespace os
{
const nx::utils::OsInfo ubuntu("linux_x64", "ubuntu");
const nx::utils::OsInfo ubuntu14("linux_x64", "ubuntu", "14.04");
const nx::utils::OsInfo ubuntu16("linux_x64", "ubuntu", "16.04");
const nx::utils::OsInfo ubuntu18("linux_x64", "ubuntu", "18.04");
const nx::utils::OsInfo windows("windows_x64");
} // namespace os;

namespace nx::vms::client::desktop {

// virtual void SetUp() will be called before each test is run.
void ClientUpdateTestEnvironment::SetUp()
{
    m_runtime.reset(new QnClientRuntimeSettings(QnStartupParameters()));
    m_staticCommon.reset(new QnStaticCommonModule());
    m_module.reset(new QnClientCoreModule(QnClientCoreModule::Mode::unitTests));
    m_resourceRuntime.reset(new QnResourceRuntimeDataManager(m_module->commonModule()));
    //m_accessController.reset(new QnWorkbenchAccessController(m_module->commonModule()));
}

// virtual void TearDown() will be called after each test is run.
void ClientUpdateTestEnvironment::TearDown()
{
    //m_currentUser.clear();
    //m_accessController.clear();
    m_resourceRuntime.clear();
    m_module.clear();
    m_staticCommon.reset();
    m_runtime.clear();
}

ClientVerificationData ClientUpdateTestEnvironment::makeClientData(nx::utils::SoftwareVersion version)
{
    ClientVerificationData data;
    data.osInfo = os::windows;
    data.currentVersion = version;
    data.clientId = QnUuid("cccccccc-cccc-cccc-cccc-cccccccccccc");
    return data;
}

QnMediaServerResourcePtr ClientUpdateTestEnvironment::makeServer(nx::utils::SoftwareVersion version, bool online)
{
    QnMediaServerResourcePtr server(new QnMediaServerResource(commonModule()));
    server->setVersion(version);
    server->setIdUnsafe(QnUuid::createUuid());
    server->setOsInfo(os::windows);
    server->setStatus(online ? nx::vms::api::ResourceStatus::online : nx::vms::api::ResourceStatus::offline);

    resourcePool()->addResource(server);
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

QnCommonModule* ClientUpdateTestEnvironment::commonModule() const
{
    return m_module->commonModule();
}

QnResourcePool* ClientUpdateTestEnvironment::resourcePool() const
{
    return commonModule()->resourcePool();
}

} // namespace nx::vms::client::desktop
