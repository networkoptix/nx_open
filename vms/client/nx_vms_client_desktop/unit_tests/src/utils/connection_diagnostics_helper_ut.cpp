// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <chrono>
#include <optional>

#include <gtest/gtest.h>

#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/network/socket_global.h>
#include <nx/vms/api/data/module_information.h>
#include <nx/vms/client/core/network/logon_data.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/test_support/test_context.h>
#include <nx/vms/client/desktop/utils/mime_data.h>
#include <nx/vms/common/resource/camera_resource_stub.h>
#include <nx/vms/utils/system_uri.h>
#include <utils/connection_diagnostics_helper.h>

namespace nx::vms::client::desktop::test {

namespace {

using namespace std::chrono;
using nx::vms::api::UserType;
using nx::vms::utils::SystemUri;

const QString kCloudSystemId("d0b73d03-3e2e-405d-8226-019c83b13a08");
const nx::utils::SoftwareVersion kFirstVersionWithSystemUri(6, 1, 4, 0);
const nx::utils::SoftwareVersion kLastVersionWithoutSystemUri(6, 1, 3, 99999);
constexpr milliseconds kTimestamp(1520110800000);

} // namespace

class CompatibleClientParametersTest: public ContextBasedTest
{
protected:
    /** Cameras must be in the resource pool to be restored from the delayed drop. */
    UuidList givenCameras(int count)
    {
        const auto server = addServer();

        UuidList result;
        for (int i = 0; i < count; ++i)
        {
            QnVirtualCameraResourcePtr camera(new CameraResourceStub());
            camera->setIdUnsafe(nx::Uuid::createUuid());
            camera->setParentId(server->getId());
            systemContext()->resourcePool()->addResource(camera);
            result.push_back(camera->getId());
        }
        return result;
    }

    static nx::vms::api::ModuleInformation server(
        const nx::utils::SoftwareVersion& version, const QString& cloudSystemId = kCloudSystemId)
    {
        nx::vms::api::ModuleInformation result;
        result.version = version;
        result.cloudSystemId = cloudSystemId;
        return result;
    }

    static core::LogonData logonData(UserType userType, std::optional<UuidList> resourceIds)
    {
        core::LogonData result;
        result.userType = userType;
        if (resourceIds)
        {
            result.delayedDrops =
                core::DelayedDrops{.resourceIds = *resourceIds, .timestamp = kTimestamp};
        }
        return result;
    }

    static void thenDelayedDropOpens(const QStringList& params, const UuidList& resourceIds)
    {
        ASSERT_EQ(2, params.size());
        ASSERT_EQ(QString("--delayed-drop"), params[0]);

        // Restore the same way as the compatible client does.
        const MimeData mimeData(QByteArray::fromBase64(params[1].toLatin1()));

        UuidList restoredIds;
        for (const auto& resource: mimeData.resources())
            restoredIds.push_back(resource->getId());
        ASSERT_EQ(resourceIds, restoredIds);
    }

private:
    // The System URI is created using the Cloud host of the client.
    nx::network::SocketGlobals::InitGuard m_socketGlobals;
};

TEST_F(CompatibleClientParametersTest, noParametersWithoutDelayedDrops)
{
    for (const auto& logon: {logonData(UserType::cloud, /*resourceIds*/ std::nullopt),
             logonData(UserType::cloud, UuidList())})
    {
        const auto [params, systemUri] = QnConnectionDiagnosticsHelper::compatibleClientParameters(
            server(kFirstVersionWithSystemUri), logon);
        EXPECT_TRUE(params.isEmpty());
        EXPECT_TRUE(systemUri.isEmpty());
    }
}

TEST_F(CompatibleClientParametersTest, systemUriForCloudSystemOfSupportingVersion)
{
    const UuidList resourceIds{nx::Uuid::createUuid(), nx::Uuid::createUuid()};

    const auto [params, systemUri] = QnConnectionDiagnosticsHelper::compatibleClientParameters(
        server(kFirstVersionWithSystemUri), logonData(UserType::cloud, resourceIds));
    EXPECT_TRUE(params.isEmpty());

    const SystemUri uri(systemUri);
    EXPECT_EQ(SystemUri::Scope::generic, uri.scope);
    EXPECT_EQ(SystemUri::ClientCommand::Client, uri.clientCommand);
    EXPECT_EQ(kCloudSystemId, uri.systemAddress);
    EXPECT_EQ(resourceIds, uri.resourceIds);
    EXPECT_EQ(kTimestamp.count(), uri.timestamp);
    EXPECT_TRUE(uri.authKey().isEmpty()) << systemUri.toStdString();
    EXPECT_TRUE(uri.authCode.isEmpty()) << systemUri.toStdString();
    EXPECT_TRUE(uri.isValid(/*requireAuthForCloudSystemConnection*/ false))
        << systemUri.toStdString();
    EXPECT_FALSE(uri.isValid()) << systemUri.toStdString();
}

TEST_F(CompatibleClientParametersTest, delayedDropForCloudSystemOfOlderVersion)
{
    const auto resourceIds = givenCameras(2);

    const auto [params, systemUri] = QnConnectionDiagnosticsHelper::compatibleClientParameters(
        server(kLastVersionWithoutSystemUri), logonData(UserType::cloud, resourceIds));
    EXPECT_TRUE(systemUri.isEmpty());
    thenDelayedDropOpens(params, resourceIds);
}

TEST_F(CompatibleClientParametersTest, delayedDropWithoutCloudSystemId)
{
    const auto resourceIds = givenCameras(1);

    const auto [params, systemUri] = QnConnectionDiagnosticsHelper::compatibleClientParameters(
        server(kFirstVersionWithSystemUri, /*cloudSystemId*/ QString()),
        logonData(UserType::cloud, resourceIds));
    EXPECT_TRUE(systemUri.isEmpty());
    thenDelayedDropOpens(params, resourceIds);
}

TEST_F(CompatibleClientParametersTest, delayedDropForLocalUser)
{
    const auto resourceIds = givenCameras(1);

    const auto [params, systemUri] = QnConnectionDiagnosticsHelper::compatibleClientParameters(
        server(kFirstVersionWithSystemUri), logonData(UserType::local, resourceIds));
    EXPECT_TRUE(systemUri.isEmpty());
    thenDelayedDropOpens(params, resourceIds);
}

} // namespace nx::vms::client::desktop::test
