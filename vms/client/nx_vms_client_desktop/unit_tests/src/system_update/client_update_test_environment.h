#pragma once

#include <gtest/gtest.h>

#include <common/common_module.h>
#include <common/static_common_module.h>
#include <core/resource/media_server_resource.h>

#include "nx/vms/client/desktop/system_update/update_verification.h"

class QnClientCoreModule;
class QnClientRuntimeSettings;
class QnResourceRuntimeDataManager;

namespace os
{
extern const nx::utils::OsInfo ubuntu;
extern const nx::utils::OsInfo ubuntu14;
extern const nx::utils::OsInfo ubuntu16;
extern const nx::utils::OsInfo ubuntu18;
extern const nx::utils::OsInfo windows;
} // namespace os

namespace nx::vms::client::desktop {

class ClientUpdateTestEnvironment: public testing::Test
{
public:
    // virtual void SetUp() will be called before each test is run.
    virtual void SetUp();

    // virtual void TearDown() will be called after each test is run.
    virtual void TearDown();

    ClientVerificationData makeClientData(nx::utils::SoftwareVersion version);

    QnMediaServerResourcePtr makeServer(nx::utils::SoftwareVersion version, bool online = true);

    std::map<QnUuid, QnMediaServerResourcePtr> getAllServers();

    void removeAllServers();

    QnCommonModule* commonModule() const;
    QnResourcePool* resourcePool() const;

    QScopedPointer<QnStaticCommonModule> m_staticCommon;
    QSharedPointer<QnClientCoreModule> m_module;
    QSharedPointer<QnClientRuntimeSettings> m_runtime;
    QSharedPointer<QnResourceRuntimeDataManager> m_resourceRuntime;
};

using Version = nx::utils::SoftwareVersion;

} // namespace nx::vms::client::desktop
