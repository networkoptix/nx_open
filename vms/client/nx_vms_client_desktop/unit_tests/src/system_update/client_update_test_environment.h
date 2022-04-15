// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <gtest/gtest.h>

#include <core/resource/media_server_resource.h>
#include <nx/vms/client/desktop/system_update/update_verification.h>
#include <nx/vms/client/desktop/test_support/test_context.h>

class QnResourceRuntimeDataManager;

namespace os
{
extern const nx::utils::OsInfo ubuntu;
extern const nx::utils::OsInfo ubuntu14;
extern const nx::utils::OsInfo ubuntu16;
extern const nx::utils::OsInfo ubuntu18;
extern const nx::utils::OsInfo windows;
} // namespace os

namespace nx::vms::client::desktop::test {

class ClientUpdateTestEnvironment: public ContextBasedTest
{
public:
    // virtual void SetUp() will be called before each test is run.
    virtual void SetUp();

    // virtual void TearDown() will be called after each test is run.
    virtual void TearDown();

    ClientVerificationData makeClientData(nx::utils::SoftwareVersion version);

    QnMediaServerResourcePtr makeServer(
        nx::utils::SoftwareVersion version,
        bool online = true,
        bool compatibleServer = true);

    std::map<QnUuid, QnMediaServerResourcePtr> getAllServers();

    void removeAllServers();

    QnResourcePool* resourcePool() const;

    QSharedPointer<QnResourceRuntimeDataManager> m_resourceRuntime;
};

using Version = nx::utils::SoftwareVersion;

} // namespace nx::vms::client::desktop::test
