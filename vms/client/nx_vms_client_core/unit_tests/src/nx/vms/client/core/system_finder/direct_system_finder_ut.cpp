// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <QtTest/QSignalSpy>

#include <nx/vms/client/core/system_finder/private/direct_system_finder.h>

#include "mock_module_discovery_manager.h"

namespace nx::vms::client::core::test {

namespace {

const nx::Uuid kLocalId1 = nx::Uuid::createUuid();
const nx::Uuid kServerId1 = nx::Uuid::createUuid();
const nx::Uuid kServerId2 = nx::Uuid::createUuid();

nx::vms::discovery::ModuleEndpoint createModuleEndpoint(
    const nx::Uuid& serverId,
    const nx::Uuid& localSystemId,
    const QString& systemName = "Test System",
    const nx::network::SocketAddress& endpoint = {"192.168.1.1", 7001})
{
    nx::vms::discovery::ModuleEndpoint module;
    module.id = serverId;
    module.localSystemId = localSystemId;
    module.systemName = systemName;
    module.version = nx::utils::SoftwareVersion(6, 0, 0, 0);
    module.endpoint = endpoint;
    return module;
}

} // namespace

template<typename T = void>
class DirectSystemFinderTestBase: public ::testing::Test
{
public:
    DirectSystemFinderTestBase()
    {
    }

    void givenDirectSystemFinder()
    {
        mockManager = std::make_unique<MockModuleDiscoveryManager>();
        finder = std::make_unique<DirectSystemFinder>(mockManager.get(), nullptr);
    }

    void whenServerFound(const nx::vms::discovery::ModuleEndpoint& module)
    {
        mockManager->emitFound(module);
    }

    void whenServerChanged(const nx::vms::discovery::ModuleEndpoint& module)
    {
        mockManager->emitChanged(module);
    }

    void whenServerLost(const nx::Uuid& serverId)
    {
        mockManager->emitLost(serverId);
    }

    QString systemIdFromLocalId(const nx::Uuid& localId) const
    {
        return localId.toSimpleString();
    }

    void thenSystemCountIs(int count)
    {
        ASSERT_EQ(finder->systems().count(), count);
    }

    void thenSystemExists(const QString& systemId)
    {
        const auto system = finder->getSystem(systemId);
        ASSERT_NE(system, nullptr);
        ASSERT_EQ(system->id(), systemId);
    }

    void thenSystemDoesNotExist(const QString& systemId)
    {
        const auto system = finder->getSystem(systemId);
        ASSERT_EQ(system, nullptr);
    }

    void thenSystemIsReachable(const QString& systemId, bool expectedReachable)
    {
        const auto system = finder->getSystem(systemId);
        ASSERT_NE(system, nullptr);
        ASSERT_EQ(system->isReachable(), expectedReachable);
    }

    void thenSystemHasServersCount(const QString& systemId, int expectedCount)
    {
        const auto system = finder->getSystem(systemId);
        ASSERT_NE(system, nullptr);
        ASSERT_EQ(system->servers().count(), expectedCount);
    }

    void thenSystemContainsServer(const QString& systemId, const nx::Uuid& serverId)
    {
        const auto system = finder->getSystem(systemId);
        ASSERT_NE(system, nullptr);
        ASSERT_TRUE(system->containsServer(serverId));
    }

    void thenSystemDoesNotContainServer(const QString& systemId, const nx::Uuid& serverId)
    {
        const auto system = finder->getSystem(systemId);
        ASSERT_NE(system, nullptr);
        ASSERT_FALSE(system->containsServer(serverId));
    }

public:
    std::unique_ptr<MockModuleDiscoveryManager> mockManager;
    std::unique_ptr<DirectSystemFinder> finder;
};

using DirectSystemFinderTest = DirectSystemFinderTestBase<>;

//-------------------------------------------------------------------------------------------------
// Basic discovery tests.
//-------------------------------------------------------------------------------------------------

TEST_F(DirectSystemFinderTest, noSystemsByDefault)
{
    givenDirectSystemFinder();

    thenSystemCountIs(0);
}

TEST_F(DirectSystemFinderTest, discoversSystemWhenServerFound)
{
    givenDirectSystemFinder();

    whenServerFound(createModuleEndpoint(kServerId1, kLocalId1));

    const QString systemId = systemIdFromLocalId(kLocalId1);
    thenSystemCountIs(1);
    thenSystemExists(systemId);
    thenSystemContainsServer(systemId, kServerId1);
    thenSystemIsReachable(systemId, true);
}

TEST_F(DirectSystemFinderTest, removesSystemWhenLastServerLost)
{
    givenDirectSystemFinder();

    whenServerFound(createModuleEndpoint(kServerId1, kLocalId1));
    const QString systemId = systemIdFromLocalId(kLocalId1);
    thenSystemCountIs(1);

    whenServerLost(kServerId1);

    thenSystemCountIs(0);
    thenSystemDoesNotExist(systemId);
}

//-------------------------------------------------------------------------------------------------
// Multi-server tests for DirectSystemFinder
//-------------------------------------------------------------------------------------------------

TEST_F(DirectSystemFinderTest, systemWithTwoServersRemainsAfterOneLost)
{
    givenDirectSystemFinder();

    // Discover system with first server.
    whenServerFound(createModuleEndpoint(kServerId1, kLocalId1, "Test System", {"192.168.1.1", 7001}));

    const QString systemId = systemIdFromLocalId(kLocalId1);
    thenSystemCountIs(1);
    thenSystemIsReachable(systemId, true);
    thenSystemHasServersCount(systemId, 1);

    // Add second server to the same system.
    whenServerFound(createModuleEndpoint(kServerId2, kLocalId1, "Test System", {"192.168.1.2", 7001}));

    thenSystemCountIs(1);
    thenSystemIsReachable(systemId, true);
    thenSystemHasServersCount(systemId, 2);
    thenSystemContainsServer(systemId, kServerId1);
    thenSystemContainsServer(systemId, kServerId2);

    // Lose first server - system should still exist with one server.
    whenServerLost(kServerId1);

    thenSystemCountIs(1);
    thenSystemHasServersCount(systemId, 1);
    thenSystemIsReachable(systemId, true);
    thenSystemDoesNotContainServer(systemId, kServerId1);
    thenSystemContainsServer(systemId, kServerId2);
}

TEST_F(DirectSystemFinderTest, systemRemovedWhenAllServersLost)
{
    givenDirectSystemFinder();

    // Discover system with two servers.
    whenServerFound(createModuleEndpoint(kServerId1, kLocalId1, "Test System", {"192.168.1.1", 7001}));
    whenServerFound(createModuleEndpoint(kServerId2, kLocalId1, "Test System", {"192.168.1.2", 7001}));

    const QString systemId = systemIdFromLocalId(kLocalId1);
    thenSystemCountIs(1);
    thenSystemHasServersCount(systemId, 2);

    // Lose both servers.
    whenServerLost(kServerId1);
    thenSystemCountIs(1);
    thenSystemHasServersCount(systemId, 1);

    whenServerLost(kServerId2);
    thenSystemCountIs(0);
    thenSystemDoesNotExist(systemId);
}

//-------------------------------------------------------------------------------------------------
// Signal emission tests.
//-------------------------------------------------------------------------------------------------

TEST_F(DirectSystemFinderTest, emitsSystemDiscoveredSignal)
{
    givenDirectSystemFinder();

    QSignalSpy spy(finder.get(), &DirectSystemFinder::systemDiscovered);

    whenServerFound(createModuleEndpoint(kServerId1, kLocalId1));

    ASSERT_EQ(spy.count(), 1);
}

TEST_F(DirectSystemFinderTest, emitsSystemLostSignalWhenLastServerLost)
{
    givenDirectSystemFinder();

    whenServerFound(createModuleEndpoint(kServerId1, kLocalId1));

    QSignalSpy spy(finder.get(), &DirectSystemFinder::systemLost);

    whenServerLost(kServerId1);

    ASSERT_EQ(spy.count(), 1);
}

TEST_F(DirectSystemFinderTest, doesNotEmitSystemLostWhenOtherServersRemain)
{
    givenDirectSystemFinder();

    whenServerFound(createModuleEndpoint(kServerId1, kLocalId1, "Test System", {"192.168.1.1", 7001}));
    whenServerFound(createModuleEndpoint(kServerId2, kLocalId1, "Test System", {"192.168.1.2", 7001}));

    QSignalSpy spy(finder.get(), &DirectSystemFinder::systemLost);

    whenServerLost(kServerId1);

    ASSERT_EQ(spy.count(), 0);
}

TEST_F(DirectSystemFinderTest, doesNotEmitSystemDiscoveredForSecondServer)
{
    givenDirectSystemFinder();

    whenServerFound(createModuleEndpoint(kServerId1, kLocalId1, "Test System", {"192.168.1.1", 7001}));

    QSignalSpy spy(finder.get(), &DirectSystemFinder::systemDiscovered);

    whenServerFound(createModuleEndpoint(kServerId2, kLocalId1, "Test System", {"192.168.1.2", 7001}));

    ASSERT_EQ(spy.count(), 0);
}

} // namespace nx::vms::client::core::test
