// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <network/system_helpers.h>
#include <nx/vms/client/core/application_context.h>
#include <nx/vms/client/core/system_finder/private/cloud_system_description.h>
#include <nx/vms/client/core/system_finder/private/local_system_description.h>
#include <nx/vms/client/core/system_finder/private/system_description_aggregator.h>
#include <nx/vms/client/core/system_finder/system_finder.h>
#include <nx/vms/common/network/server_compatibility_validator.h>
#include <ui/models/system_hosts_model.h>

#include "system_finder_mock.h"

namespace nx::vms::client::core::test {

namespace {

const QString kCloudId1 = nx::Uuid::createUuid().toString(QUuid::WithBraces);
const QString kCloudId2 = nx::Uuid::createUuid().toString(QUuid::WithBraces);
const nx::Uuid kLocalId1 = nx::Uuid::createUuid();
const nx::Uuid kLocalId2 = nx::Uuid::createUuid();
const auto kServerId1 = nx::Uuid::createUuid();
const auto kServerUrl1 = nx::Url("https://localhost:7001");

} // namespace

class QnSystemFinderMock: public SystemFinder
{
public:
    SystemFinderMockPtr cloudFinder{std::make_unique<SystemFinderMock>()};
    SystemFinderMockPtr directFinder{std::make_unique<SystemFinderMock>()};
    SystemFinderMockPtr recentFinder{std::make_unique<SystemFinderMock>()};
    SystemFinderMockPtr scopeFinder{std::make_unique<SystemFinderMock>()};

    QnSystemFinderMock():
        SystemFinder(/*addDefaultFinders*/ false, /*parent*/ nullptr)
    {
        addSystemFinder(cloudFinder.get(), Source::cloud);
        addSystemFinder(directFinder.get(), Source::direct);
        addSystemFinder(recentFinder.get(), Source::recent);
        addSystemFinder(scopeFinder.get(), Source::saved);
    }
};

class SystemFinderTest: public ::testing::Test
{
public:
    SystemFinderTest()
    {
        appContext = std::make_unique<ApplicationContext>(
            ApplicationContext::Mode::unitTests,
            Qn::SerializationFormat::json,
            nx::vms::api::PeerType::desktopClient);
        systemFinder = std::make_unique<QnSystemFinderMock>();

        nx::vms::common::ServerCompatibilityValidator::initialize(
            nx::vms::common::ServerCompatibilityValidator::Peer::desktopClient,
            Qn::SerializationFormat::json,
            /*ignoreAll*/ {-1});
    }

    /** Have a system listed in the shared cloud systems list. */
    void givenCloudSystem(const QnCloudSystem& system)
    {
        systemFinder->cloudFinder->addSystem(QnCloudSystemDescription::create(system));
    }

    void whenCloudSystemLost(const QString& id)
    {
        systemFinder->cloudFinder->removeSystem(id);
    }

    /** Have a system which is accessible locally. */
    void givenAccessibleSystem(const api::ModuleInformation& module)
    {
        const QString systemId = helpers::getTargetSystemId(module);
        const nx::Uuid localId = helpers::getLocalSystemId(module);
        auto system = LocalSystemDescription::create(
            systemId,
            localId,
            module.cloudSystemId,
            "Direct Accessible System");

        if (!module.id.isNull())
        {
            system->addServer(module, /*online*/ true);
            system->setServerHost(module.id, kServerUrl1);
        }

        systemFinder->directFinder->addSystem(system);
    }

    void whenDirectSystemLost(const QString& id)
    {
        systemFinder->directFinder->removeSystem(id);
    }

    /** Have a record about a system we connected recently (only local id is stored). */
    void givenRecentSystem(nx::Uuid localId)
    {
        constexpr auto name = "Recent System";

        auto system = LocalSystemDescription::create(
            localId.toSimpleString(), localId, /*cloudSystemId*/ QString(), name);

        // Recent system finder does not store server ids, so it adds a fake ones.
        nx::vms::api::ModuleInformationWithAddresses fakeServerInfo;
        fakeServerInfo.id = nx::Uuid::createUuid();   //< It should be new unique id.
        fakeServerInfo.systemName = name;
        fakeServerInfo.version = nx::utils::SoftwareVersion(5, 1, 0, 0);

        system->addServer(fakeServerInfo, /*online*/ false);
        system->setServerHost(fakeServerInfo.id, kServerUrl1);

        systemFinder->recentFinder->addSystem(system);
    }

    void whenLogoutFromCloud()
    {
        systemFinder->cloudFinder->clear();
    }

    void thenSystemCountIs(int count)
    {
        ASSERT_EQ(systems().count(), count);
    }

    void thenFirstServerId(const QString& id, nx::Uuid localId, nx::Uuid serverId)
    {
        QnSystemHostsModel shm(nullptr, systemFinder.get());
        shm.setSystemId(id);
        shm.setLocalSystemId(localId.toQUuid());

        EXPECT_EQ(serverId, shm.index(0, 0).data(QnSystemHostsModel::ServerIdRole).toUuid());
    }

    SystemDescriptionList systems() const { return systemFinder->systems(); }

    SystemDescriptionAggregatorPtr aggregator(int i) const
    {
        return systems().value(i).dynamicCast<SystemDescriptionAggregator>();
    }

public:
    std::unique_ptr<ApplicationContext> appContext;
    std::unique_ptr<QnSystemFinderMock> systemFinder;
};

TEST_F(SystemFinderTest, noSystemsByDefault)
{
    thenSystemCountIs(0);
}

// VMS-53593: Multiple online cloud systems with the same local id should not be merged.
TEST_F(SystemFinderTest, offlineCCloudSystemsWithSameLocalIdAreDisplayedSeparately)
{
    givenCloudSystem({.cloudId = kCloudId1, .localId = kLocalId1, .online = true});
    givenCloudSystem({.cloudId = kCloudId2, .localId = kLocalId1, .online = false});

    thenSystemCountIs(2);
}

// VMS-53593: Multiple online cloud systems with the same local id should not be merged.
TEST_F(SystemFinderTest, manyCloudSystemsWithSameLocalIdAreDisplayedSeparately)
{
    givenCloudSystem({.cloudId = kCloudId1, .localId = kLocalId1, .online = true});
    givenCloudSystem({.cloudId = kCloudId2, .localId = kLocalId1, .online = true});
    thenSystemCountIs(2);
}

// VMS-53593: Multiple online cloud systems with the same local id should not be merged.
TEST_F(SystemFinderTest, manyCloudBoundButLocalSystemsWithSameLocalIdAreDisplayedSeparately)
{
    givenAccessibleSystem({.cloudSystemId = kCloudId1, .localSystemId = kLocalId1});
    givenAccessibleSystem({.cloudSystemId = kCloudId2, .localSystemId = kLocalId1});
    thenSystemCountIs(2);
}

// VMS-53593: Multiple online cloud systems with the same local id should not be merged.
TEST_F(SystemFinderTest, cloudSystemShouldNotBeMergedWithLocalWithSameLocalIdIfFoundEarlier)
{
    givenAccessibleSystem({.cloudSystemId = kCloudId1, .localSystemId = kLocalId1});
    givenAccessibleSystem({.localSystemId = kLocalId1});
    thenSystemCountIs(2);
}

// VMS-53593: Multiple online cloud systems with the same local id should not be merged.
TEST_F(SystemFinderTest, cloudSystemShouldNotBeMergedWithLocalWithSameLocalIdIfFoundLater)
{
    givenAccessibleSystem({.localSystemId = kLocalId1});
    givenAccessibleSystem({.cloudSystemId = kCloudId1, .localSystemId = kLocalId1});
    thenSystemCountIs(2);
}

// VMS-47379, VMS-39653: Recent local system is connected to cloud.
TEST_F(SystemFinderTest, recentLocalSystemWasConnectedToCloudWhileTheClientWasOffline)
{
    // When we connected to some system earlier while it was local.
    givenRecentSystem(kLocalId1);
    // And then it was connected to the cloud.
    givenAccessibleSystem({.cloudSystemId = kCloudId1, .localSystemId = kLocalId1});
    // Then systems should be merged.
    thenSystemCountIs(1);
}

TEST_F(SystemFinderTest, connectToCloudSystemInLocalMode)
{
    // When we bound system to the cloud.
    givenAccessibleSystem({.cloudSystemId = kCloudId1, .localSystemId = kLocalId1});
    // And then saved connection to this system.
    givenRecentSystem(kLocalId1);
    // Then systems should be merged.
    thenSystemCountIs(1);
}

// VMS-53746: Disconnecting from the cloud should not make systems disappear.
TEST_F(SystemFinderTest, disconnectFromCloudWhileSystemIsAvailableLocally)
{
    // System is available locally, so we know all its ids.
    givenAccessibleSystem({.cloudSystemId = kCloudId1, .localSystemId = kLocalId1});
    // When user logged into the cloud, we got all the system info.
    givenCloudSystem({.cloudId = kCloudId1, .localId = kLocalId1, .online = true});
    // Then systems should be merged.
    thenSystemCountIs(1);

    // When we logged out from cloud, system should still be discoverable.
    whenLogoutFromCloud();
    thenSystemCountIs(1);
}

// VMS-53746: Disconnecting from the cloud should not make systems disappear.
TEST_F(SystemFinderTest, disconnectFromCloudWhileConnectedSystemIsAvailableLocally)
{
    // System is known as connected earlier.
    givenRecentSystem(kLocalId1);
    // System is available locally, so we know all its ids.
    givenAccessibleSystem({.cloudSystemId = kCloudId1, .localSystemId = kLocalId1});
    // When user logged into the cloud, we got all the system info.
    givenCloudSystem({.cloudId = kCloudId1, .localId = kLocalId1, .online = true});
    // Then systems should be merged.
    thenSystemCountIs(1);

    // When we logged out from cloud, system should still be discoverable.
    whenLogoutFromCloud();
    thenSystemCountIs(1);
}

// VMS-53746: Disconnecting from the cloud should not make systems disappear.
TEST_F(SystemFinderTest, disconnectFromCloudWhileTwoSystemsWithSameIdAreAvailableLocally)
{
    // Systems are available locally, so we know all its ids.
    givenAccessibleSystem({.cloudSystemId = kCloudId1, .localSystemId = kLocalId1});
    givenAccessibleSystem({.cloudSystemId = kCloudId2, .localSystemId = kLocalId1});
    // When user logged into the cloud, we got all the system info.
    givenCloudSystem({.cloudId = kCloudId1, .localId = kLocalId1, .online = true});
    givenCloudSystem({.cloudId = kCloudId2, .localId = kLocalId1, .online = true});
    // Both system tiles are displayed correctly and merged one by one.
    thenSystemCountIs(2);

    // When we logged out from cloud, system should still be discoverable.
    whenLogoutFromCloud();
    thenSystemCountIs(2);
}

// VMS-53746: Disconnecting from the cloud should not make systems disappear.
TEST_F(SystemFinderTest, disconnectFromCloudWhileTwoConnectedSystemsWithSameIdAreAvailableLocally)
{
    // At least one of systems is known as connected earlier - but the local is the same.
    givenRecentSystem(kLocalId1);
    // Systems are available locally, so we know all its ids.
    givenAccessibleSystem({.cloudSystemId = kCloudId1, .localSystemId = kLocalId1});
    givenAccessibleSystem({.cloudSystemId = kCloudId2, .localSystemId = kLocalId1});
    // When user logged into the cloud, we got all the system info.
    givenCloudSystem({.cloudId = kCloudId1, .localId = kLocalId1, .online = true});
    givenCloudSystem({.cloudId = kCloudId2, .localId = kLocalId1, .online = true});
    // Both system tiles are displayed correctly and merged one by one.
    thenSystemCountIs(2);

    // When we logged out from cloud, system should still be discoverable.
    whenLogoutFromCloud();
    thenSystemCountIs(2);
}

// VMS-53746: Disconnecting from the cloud should not make systems disappear even if they online.
TEST_F(SystemFinderTest, disconnectFromCloudShouldNotClearOfflineSystems)
{
    // We know about local system existence, which is offline right now.
    givenRecentSystem(kLocalId1);
    // Then user connects to the cloud and gets the same system info, including cloud id.
    givenCloudSystem({.cloudId = kCloudId1, .localId = kLocalId1, .online = false});
    thenSystemCountIs(1);
    whenLogoutFromCloud();

    // Recent system info should still be available.
    thenSystemCountIs(1);
}

TEST_F(SystemFinderTest, unbindFromCloudDoesNotLeaveStaleCloudTile)
{
    auto moduleInfo = nx::vms::api::ModuleInformation{
        .cloudSystemId = kCloudId1, .localSystemId = kLocalId1};
    moduleInfo.id = kServerId1;

    // System is known from a previous local connection.
    givenRecentSystem(kLocalId1);
    // System is accessible locally and currently cloud-bound.
    givenAccessibleSystem(moduleInfo);
    // Cloud also reports the system.
    givenCloudSystem({.cloudId = kCloudId1, .localId = kLocalId1, .online = true});
    thenSystemCountIs(1);

    // Site disconnects from cloud: the direct finder loses the cloud-id system first ...
    whenDirectSystemLost(kCloudId1);
    // ... and re-discovers it under the local id (cloudSystemId is empty now).
    moduleInfo.cloudSystemId.clear();
    givenAccessibleSystem(moduleInfo);
    // Then the cloud finder also loses it.
    whenCloudSystemLost(kCloudId1);

    // Only one tile must remain - the stale aggregator under the old cloud id must be gone.
    thenSystemCountIs(1);
    const auto first = systems().first();
    EXPECT_EQ(first->id(), kLocalId1.toSimpleString());
    EXPECT_EQ(first->localId(), kLocalId1);

    // The system contains the real server and recent system is merged correctly.
    thenFirstServerId(first->id(), first->localId(), kServerId1);
    EXPECT_EQ(aggregator(0)->systemCount(), 2);
}

} // namespace nx::vms::client::core::test
