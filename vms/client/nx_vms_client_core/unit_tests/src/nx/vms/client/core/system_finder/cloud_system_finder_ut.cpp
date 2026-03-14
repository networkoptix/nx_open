// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <QtTest/QSignalSpy>

#include <nx/kit/ini_config.h>
#include <nx/vms/client/core/application_context.h>
#include <nx/vms/client/core/ini.h>
#include <nx/vms/client/core/system_finder/private/cloud_system_description.h>
#include <nx/vms/client/core/system_finder/private/cloud_system_finder.h>

#include "mock_cloud_status_watcher.h"
#include "test_cloud_system_finder.h"

namespace nx::vms::client::core::test {

namespace {

const QString kCloudId1 = nx::Uuid::createUuid().toString(QUuid::WithBraces);
const QString kCloudId2 = nx::Uuid::createUuid().toString(QUuid::WithBraces);
const nx::Uuid kLocalId1 = nx::Uuid::createUuid();
const nx::Uuid kLocalId2 = nx::Uuid::createUuid();
const nx::Uuid kServerId1 = nx::Uuid::createUuid();
const nx::Uuid kServerId2 = nx::Uuid::createUuid();
const nx::Uuid kTargetServerId = nx::Uuid::createUuid();

QnCloudSystem createCloudSystem(
    const QString& cloudId,
    const nx::Uuid& localId,
    bool online = true)
{
    QnCloudSystem system;
    system.cloudId = cloudId;
    system.localId = localId;
    system.online = online;
    system.system2faEnabled = false;
    return system;
}

nx::vms::api::ModuleInformationWithAddresses createServerInfo(
    const nx::Uuid& serverId,
    const QString& cloudSystemId,
    const nx::Uuid& localSystemId)
{
    nx::vms::api::ModuleInformationWithAddresses server;
    server.id = serverId;
    server.cloudSystemId = cloudSystemId;
    server.localSystemId = localSystemId;
    server.version = nx::utils::SoftwareVersion(6, 0, 0, 0);
    return server;
}

} // namespace

class CloudSystemFinderTest: public ::testing::Test
{
public:
    CloudSystemFinderTest()
    {
        iniTweaks.set(&ini().doNotPingCloudSystems, true);

        appContext = std::make_unique<ApplicationContext>(
            ApplicationContext::Mode::unitTests,
            Qn::SerializationFormat::json,
            nx::vms::api::PeerType::desktopClient);
    }

    void givenCloudSystemFinderWhenLoggedOut()
    {
        watcher = std::make_unique<MockCloudStatusWatcher>();
        finder = std::make_unique<TestCloudSystemFinder>(watcher.get());
    }

    void givenCloudSystemFinder()
    {
        watcher = std::make_unique<MockCloudStatusWatcher>();
        watcher->setMockStatus(AbstractCloudStatusWatcher::Online);
        finder = std::make_unique<TestCloudSystemFinder>(watcher.get());
    }

    void whenCloudSystemsReceived(const QnCloudSystemList& systems)
    {
        watcher->setMockCloudSystems(systems);
    }

    void whenStatusChangedTo(AbstractCloudStatusWatcher::Status status)
    {
        watcher->setMockStatus(status);
    }

    void thenSystemCountIs(int count)
    {
        ASSERT_EQ(finder->systems().count(), count);
    }

    void thenSystemExists(const QString& cloudId)
    {
        const auto system = finder->getSystem(cloudId);
        ASSERT_NE(system, nullptr);
        ASSERT_EQ(system->id(), cloudId);
    }

    void thenSystemDoesNotExist(const QString& cloudId)
    {
        const auto system = finder->getSystem(cloudId);
        ASSERT_EQ(system, nullptr);
    }

    void thenSystemIsOnline(const QString& cloudId, bool expectedOnline)
    {
        const auto system = finder->getSystem(cloudId);
        ASSERT_NE(system, nullptr);
        ASSERT_EQ(system->isOnline(), expectedOnline);
    }

    void thenSystemIsReachable(const QString& cloudId, bool expectedReachable)
    {
        const auto system = finder->getSystem(cloudId);
        ASSERT_NE(system, nullptr);
        ASSERT_EQ(system->isReachable(), expectedReachable);
    }

    void thenSystemHasServersCount(const QString& cloudId, int expectedCount)
    {
        const auto system = finder->getSystem(cloudId);
        ASSERT_NE(system, nullptr);
        ASSERT_EQ(system->servers().count(), expectedCount);
    }

    void thenSystemContainsServer(const QString& cloudId, const nx::Uuid& serverId)
    {
        const auto system = finder->getSystem(cloudId);
        ASSERT_NE(system, nullptr);
        ASSERT_TRUE(system->containsServer(serverId));
    }

    void thenSystemDoesNotContainServer(const QString& cloudId, const nx::Uuid& serverId)
    {
        const auto system = finder->getSystem(cloudId);
        ASSERT_NE(system, nullptr);
        ASSERT_FALSE(system->containsServer(serverId));
    }

    QnCloudSystemDescriptionPtr getCloudSystemDescription(const QString& cloudId)
    {
        return finder->getSystem(cloudId).dynamicCast<QnCloudSystemDescription>();
    }

public:
    nx::kit::IniConfig::Tweaks iniTweaks;
    std::unique_ptr<ApplicationContext> appContext;
    std::unique_ptr<MockCloudStatusWatcher> watcher;
    std::unique_ptr<TestCloudSystemFinder> finder;
};

//-------------------------------------------------------------------------------------------------
// Basic discovery and status changes tests.
//-------------------------------------------------------------------------------------------------

TEST_F(CloudSystemFinderTest, noSystemsByDefault)
{
    givenCloudSystemFinderWhenLoggedOut();

    thenSystemCountIs(0);
}

TEST_F(CloudSystemFinderTest, discoversCloudSystemWhenOnline)
{
    givenCloudSystemFinder();

    whenCloudSystemsReceived({createCloudSystem(kCloudId1, kLocalId1)});
    thenSystemCountIs(1);
    thenSystemExists(kCloudId1);
}

TEST_F(CloudSystemFinderTest, discoversMultipleCloudSystems)
{
    givenCloudSystemFinder();

    whenCloudSystemsReceived({
        createCloudSystem(kCloudId1, kLocalId1),
        createCloudSystem(kCloudId2, kLocalId2)
    });
    thenSystemCountIs(2);
    thenSystemExists(kCloudId1);
    thenSystemExists(kCloudId2);
}

TEST_F(CloudSystemFinderTest, clearsSystemsOnLogout)
{
    givenCloudSystemFinder();

    whenCloudSystemsReceived({
        createCloudSystem(kCloudId1, kLocalId1)
    });
    thenSystemCountIs(1);

    whenStatusChangedTo(AbstractCloudStatusWatcher::LoggedOut);
    thenSystemCountIs(0);
}

TEST_F(CloudSystemFinderTest, removesSystemWhenNotInList)
{
    givenCloudSystemFinder();

    whenCloudSystemsReceived({
        createCloudSystem(kCloudId1, kLocalId1),
        createCloudSystem(kCloudId2, kLocalId2)
    });
    thenSystemCountIs(2);

    whenCloudSystemsReceived({createCloudSystem(kCloudId1, kLocalId1)});
    thenSystemCountIs(1);
    thenSystemExists(kCloudId1);
    thenSystemDoesNotExist(kCloudId2);
}

TEST_F(CloudSystemFinderTest, updatesSystemOnlineState)
{
    givenCloudSystemFinder();

    whenCloudSystemsReceived({createCloudSystem(kCloudId1, kLocalId1, /*online*/ true)});
    thenSystemIsOnline(kCloudId1, true);

    whenCloudSystemsReceived({createCloudSystem(kCloudId1, kLocalId1, /*online*/ false)});
    thenSystemIsOnline(kCloudId1, false);
}

TEST_F(CloudSystemFinderTest, emitsSystemDiscoveredSignal)
{
    givenCloudSystemFinder();

    QSignalSpy spy(finder.get(), &CloudSystemFinder::systemDiscovered);

    whenCloudSystemsReceived({createCloudSystem(kCloudId1, kLocalId1)});

    ASSERT_EQ(spy.count(), 1);
}

TEST_F(CloudSystemFinderTest, emitsSystemLostSignal)
{
    givenCloudSystemFinder();
    whenCloudSystemsReceived({createCloudSystem(kCloudId1, kLocalId1)});

    QSignalSpy spy(finder.get(), &CloudSystemFinder::systemLost);

    whenCloudSystemsReceived({});

    ASSERT_EQ(spy.count(), 1);
}

TEST_F(CloudSystemFinderTest, systemsWithSameLocalIdAreHandledSeparately)
{
    givenCloudSystemFinder();

    whenCloudSystemsReceived({
        createCloudSystem(kCloudId1, kLocalId1),
        createCloudSystem(kCloudId2, kLocalId1)  //< Same local ID.
    });

    thenSystemCountIs(2);
    thenSystemExists(kCloudId1);
    thenSystemExists(kCloudId2);
}

TEST_F(CloudSystemFinderTest, getSystemReturnsNullForUnknownId)
{
    givenCloudSystemFinder();

    whenCloudSystemsReceived({
        createCloudSystem(kCloudId1, kLocalId1),
        createCloudSystem(kCloudId2, kLocalId2)
    });
    thenSystemDoesNotExist("unknown-cloud-id");
}

TEST_F(CloudSystemFinderTest, offlineSystemIsDiscovered)
{
    givenCloudSystemFinder();

    whenCloudSystemsReceived({
        createCloudSystem(kCloudId1, kLocalId1, /*online*/ false)
    });

    thenSystemCountIs(1);
    thenSystemExists(kCloudId1);
    thenSystemIsOnline(kCloudId1, false);
}

//-------------------------------------------------------------------------------------------------
// Server-related tests for CloudSystemFinder
//-------------------------------------------------------------------------------------------------

// Test that covers VMS-61660.
TEST_F(CloudSystemFinderTest, twoSystemsWithSameServerIdBothReachable)
{
    givenCloudSystemFinder();

    // Receive two online cloud systems - each gets an initial server automatically.
    whenCloudSystemsReceived({
        createCloudSystem(kCloudId1, kLocalId1),
        createCloudSystem(kCloudId2, kLocalId2)
    });

    const auto system1 = getCloudSystemDescription(kCloudId1);
    const auto system2 = getCloudSystemDescription(kCloudId2);
    ASSERT_NE(system1, nullptr);
    ASSERT_NE(system2, nullptr);

    // Get the auto-created server IDs.
    ASSERT_EQ(system1->servers().count(), 1);
    ASSERT_EQ(system2->servers().count(), 1);
    const auto initialServerId1 = system1->servers().first().id;
    const auto initialServerId2 = system2->servers().first().id;

    // Remove initial servers and add servers with the same ID to both systems.
    system1->removeServer(initialServerId1);
    system2->removeServer(initialServerId2);

    system1->addServer(createServerInfo(kServerId1, kCloudId1, kLocalId1));
    system2->addServer(createServerInfo(kServerId1, kCloudId2, kLocalId2)); //< Same server ID.

    // Both systems should be reachable and contain this server.
    thenSystemCountIs(2);
    thenSystemIsReachable(kCloudId1, true);
    thenSystemIsReachable(kCloudId2, true);
    thenSystemContainsServer(kCloudId1, kServerId1);
    thenSystemContainsServer(kCloudId2, kServerId1);

    for (auto system: {system1, system2})
    {
        for (const auto& server: system->servers())
            finder->simulatePingResponse(server.cloudSystemId, true, server);
    }

    thenSystemContainsServer(kCloudId1, kServerId1);
    thenSystemContainsServer(kCloudId2, kServerId1);
    thenSystemIsReachable(kCloudId2, true);
}

TEST_F(CloudSystemFinderTest, systemBecomesUnreachableAfterServerDetached)
{
    givenCloudSystemFinder();

    // Online system gets an initial server automatically.
    whenCloudSystemsReceived({createCloudSystem(kCloudId1, kLocalId1)});

    const auto system = getCloudSystemDescription(kCloudId1);
    ASSERT_NE(system, nullptr);

    // System should have one auto-created server and be reachable.
    thenSystemHasServersCount(kCloudId1, 1);
    thenSystemIsReachable(kCloudId1, true);

    // Get the auto-created server ID and remove it.
    const auto initialServerId = system->servers().first().id;
    system->removeServer(initialServerId);

    // System becomes unreachable.
    thenSystemHasServersCount(kCloudId1, 0);
    thenSystemIsReachable(kCloudId1, false);
}

TEST_F(CloudSystemFinderTest, systemRemainsReachableAfterOneServerDetached)
{
    givenCloudSystemFinder();

    whenCloudSystemsReceived({createCloudSystem(kCloudId1, kLocalId1)});

    const auto system = getCloudSystemDescription(kCloudId1);
    ASSERT_NE(system, nullptr);

    // System has one auto-created server. Add another one.
    ASSERT_EQ(system->servers().count(), 1);
    const auto initialServerId = system->servers().first().id;

    system->addServer(createServerInfo(kServerId2, kCloudId1, kLocalId1));
    thenSystemIsReachable(kCloudId1, true);
    thenSystemHasServersCount(kCloudId1, 2);

    // Remove the initial server - system should remain reachable with kServerId2.
    system->removeServer(initialServerId);
    thenSystemIsReachable(kCloudId1, true);
    thenSystemDoesNotContainServer(kCloudId1, initialServerId);
    thenSystemHasServersCount(kCloudId1, 1);
    thenSystemContainsServer(kCloudId1, kServerId2);
}

TEST_F(CloudSystemFinderTest, serverMovedBetweenSystems)
{
    givenCloudSystemFinder();

    // Create two cloud systems - both get initial servers automatically.
    whenCloudSystemsReceived({
        createCloudSystem(kCloudId1, kLocalId1),
        createCloudSystem(kCloudId2, kLocalId2)
    });

    const auto systemA = getCloudSystemDescription(kCloudId1);
    const auto systemB = getCloudSystemDescription(kCloudId2);
    ASSERT_NE(systemA, nullptr);
    ASSERT_NE(systemB, nullptr);

    // Get the auto-created server IDs.
    const auto initialServerIdA = systemA->servers().first().id;
    const auto initialServerIdB = systemB->servers().first().id;

    // Remove initial server from A, keep B's server. Add target server to B.
    systemA->removeServer(initialServerIdA);
    systemB->addServer(createServerInfo(kTargetServerId, kCloudId2, kLocalId2));
    thenSystemIsReachable(kCloudId1, false); // System A - no servers
    thenSystemHasServersCount(kCloudId1, 0);
    thenSystemIsReachable(kCloudId2, true);  // System B - has servers (initial + target)
    thenSystemHasServersCount(kCloudId2, 2);

    // Move target server from B to A. Also remove B's remaining server.
    systemB->removeServer(kTargetServerId);
    systemB->removeServer(initialServerIdB);
    systemA->addServer(createServerInfo(kTargetServerId, kCloudId1, kLocalId1));
    thenSystemIsReachable(kCloudId1, true);
    thenSystemHasServersCount(kCloudId1, 1);
    thenSystemIsReachable(kCloudId2, false);
    thenSystemHasServersCount(kCloudId2, 0);
}

TEST_F(CloudSystemFinderTest, emitsReachableStateChangedOnServerChange)
{
    givenCloudSystemFinder();

    whenCloudSystemsReceived({createCloudSystem(kCloudId1, kLocalId1)});

    const auto system = getCloudSystemDescription(kCloudId1);
    ASSERT_NE(system, nullptr);

    // Get and remove the auto-created server.
    const auto initialServerId = system->servers().first().id;
    system->removeServer(initialServerId);

    QSignalSpy spy(system.get(), &SystemDescription::reachableStateChanged);

    // Add server - should emit reachableStateChanged (unreachable -> reachable).
    system->addServer(createServerInfo(kServerId1, kCloudId1, kLocalId1));
    ASSERT_EQ(spy.count(), 1);

    spy.clear();

    // Remove server - should emit reachableStateChanged (reachable -> unreachable).
    system->removeServer(kServerId1);
    ASSERT_EQ(spy.count(), 1);
}

TEST_F(CloudSystemFinderTest, serversRetainedAcrossCloudSystemUpdates)
{
    givenCloudSystemFinder();

    whenCloudSystemsReceived({createCloudSystem(kCloudId1, kLocalId1)});

    const auto system = getCloudSystemDescription(kCloudId1);
    ASSERT_NE(system, nullptr);

    // System has an initial server. Add another one.
    const auto initialServerId = system->servers().first().id;
    system->addServer(createServerInfo(kServerId1, kCloudId1, kLocalId1));
    thenSystemContainsServer(kCloudId1, initialServerId);
    thenSystemContainsServer(kCloudId1, kServerId1);
    ASSERT_EQ(system->servers().count(), 2);

    // Update cloud systems list (same system, online state changed).
    whenCloudSystemsReceived({createCloudSystem(kCloudId1, kLocalId1, /*online*/ false)});

    // System should still exist and description object should be the same.
    const auto systemAfterUpdate = getCloudSystemDescription(kCloudId1);
    ASSERT_NE(systemAfterUpdate, nullptr);
    ASSERT_EQ(system.get(), systemAfterUpdate.get());

    // Our added server should still be there.
    thenSystemContainsServer(kCloudId1, kServerId1);
}

TEST_F(CloudSystemFinderTest, offlineSystemHasNoInitialServer)
{
    givenCloudSystemFinder();

    // Offline system should not get an initial server.
    whenCloudSystemsReceived({createCloudSystem(kCloudId1, kLocalId1, /*online*/ false)});

    const auto system = getCloudSystemDescription(kCloudId1);
    ASSERT_NE(system, nullptr);

    thenSystemHasServersCount(kCloudId1, 0);
    thenSystemIsReachable(kCloudId1, false);
}

TEST_F(CloudSystemFinderTest, systemBecomesUnreachableOnFailedPing)
{
    givenCloudSystemFinder();

    whenCloudSystemsReceived({createCloudSystem(kCloudId1, kLocalId1)});

    const auto system = getCloudSystemDescription(kCloudId1);
    ASSERT_NE(system, nullptr);

    thenSystemIsReachable(kCloudId1, true);
    thenSystemHasServersCount(kCloudId1, 1);

    finder->simulatePingResponse(kCloudId1, false, {});

    thenSystemIsReachable(kCloudId1, false);
    thenSystemHasServersCount(kCloudId1, 0);
}

TEST_F(CloudSystemFinderTest, serverRemovedWhenPingReturnsWrongCloudSystemId)
{
    givenCloudSystemFinder();

    whenCloudSystemsReceived({createCloudSystem(kCloudId1, kLocalId1)});

    const auto system = getCloudSystemDescription(kCloudId1);
    ASSERT_NE(system, nullptr);

    thenSystemIsReachable(kCloudId1, true);
    thenSystemHasServersCount(kCloudId1, 1);

    const auto initialServerId = system->servers().first().id;
    auto wrongModuleInfo = createServerInfo(initialServerId, kCloudId2, kLocalId1);

    finder->simulatePingResponse(kCloudId1, true, wrongModuleInfo);

    thenSystemIsReachable(kCloudId1, false);
    thenSystemHasServersCount(kCloudId1, 0);
}

TEST_F(CloudSystemFinderTest, serverUpdatedWhenPingReturnsModifiedServerInfo)
{
    givenCloudSystemFinder();

    whenCloudSystemsReceived({createCloudSystem(kCloudId1, kLocalId1)});

    const auto system = getCloudSystemDescription(kCloudId1);
    ASSERT_NE(system, nullptr);

    thenSystemHasServersCount(kCloudId1, 1);
    const auto initialServerId = system->servers().first().id;
    const auto initialSystemName = system->servers().first().systemName;

    auto modifiedInfo = createServerInfo(initialServerId, kCloudId1, kLocalId1);
    modifiedInfo.systemName = "Modified System Name";

    finder->simulatePingResponse(kCloudId1, true, modifiedInfo);

    thenSystemIsReachable(kCloudId1, true);
    thenSystemHasServersCount(kCloudId1, 1);
    thenSystemContainsServer(kCloudId1, initialServerId);

    ASSERT_NE(system->servers().first().systemName, initialSystemName);
    ASSERT_EQ(system->servers().first().systemName, "Modified System Name");
}

} // namespace nx::vms::client::core::test
