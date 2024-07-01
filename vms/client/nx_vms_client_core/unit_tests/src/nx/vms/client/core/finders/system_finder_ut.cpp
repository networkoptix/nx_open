// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <QtCore/QCoreApplication>

#include <finders/systems_finder.h>
#include <network/cloud_system_description.h>
#include <network/local_system_description.h>
#include <network/system_helpers.h>
#include <nx/branding.h>
#include <nx/vms/client/core/application_context.h>
#include <nx/vms/client/core/settings/client_core_settings.h>
#include <nx/vms/common/network/server_compatibility_validator.h>

#include "system_finder_mock.h"

namespace nx::vms::client::core::test {

namespace {

const QString kCloudId1 = QnUuid::createUuid().toString();
const QString kCloudId2 = QnUuid::createUuid().toString();
const QnUuid kLocalId1 = QnUuid::createUuid();
const QnUuid kLocalId2 = QnUuid::createUuid();

} // namespace

using LocalSystemDescriptionPtr = QSharedPointer<QnLocalSystemDescription>;

class QnSystemsFinderMock: public QnSystemsFinder
{
public:
    SystemFinderMockPtr cloudFinder{std::make_unique<SystemFinderMock>()};
    SystemFinderMockPtr directFinder{std::make_unique<SystemFinderMock>()};
    SystemFinderMockPtr recentFinder{std::make_unique<SystemFinderMock>()};
    SystemFinderMockPtr scopeFinder{std::make_unique<SystemFinderMock>()};

    QnSystemsFinderMock():
        QnSystemsFinder(/*addDefaultFinders*/ false, /*parent*/ nullptr)
    {
        addSystemsFinder(cloudFinder.get(), Source::cloud);
        addSystemsFinder(directFinder.get(), Source::direct);
        addSystemsFinder(recentFinder.get(), Source::recent);
        addSystemsFinder(scopeFinder.get(), Source::saved);
    }
};

class SystemFinderTest: public ::testing::Test
{
public:
    SystemFinderTest()
    {
        QCoreApplication::setOrganizationName(nx::branding::company());
        QCoreApplication::setApplicationName("Unit tests");
        appContext = std::make_unique<ApplicationContext>(
            ApplicationContext::Mode::unitTests,
            nx::vms::api::PeerType::desktopClient,
            /*customCloudHost*/ QString(),
            /*ignoreCustomization*/ false);
        systemFinder = std::make_unique<QnSystemsFinderMock>();
    }

    /** Have a system listed in the shared cloud systems list. */
    void givenCloudSystem(QnCloudSystem system)
    {
        systemFinder->cloudFinder->addSystem(QnCloudSystemDescription::create(system));
    }

    /** Have a system which is accessible locally. */
    void givenAccessibleSystem(api::ModuleInformation system)
    {
        const QString systemId = helpers::getTargetSystemId(system);
        const QnUuid localId = helpers::getLocalSystemId(system);
        systemFinder->directFinder->addSystem(
            QnLocalSystemDescription::create(
                systemId,
                localId,
                system.cloudSystemId,
                "Direct Accessible System"));
    }

    /** Have a record about a system we connected recenlty (only local id is stored). */
    void givenRecentSystem(QnUuid localId)
    {
        systemFinder->recentFinder->addSystem(
            QnLocalSystemDescription::create(
                localId.toSimpleString(),
                localId,
                /*cloudSystemId*/ QString(),
                "Recent System"));
    }

    void whenLogoutFromCloud()
    {
        systemFinder->cloudFinder->clear();
    }

    void thenSystemCountIs(int count)
    {
        ASSERT_EQ(systemFinder->systems().count(), count);
    }

public:
    std::unique_ptr<ApplicationContext> appContext;
    std::unique_ptr<QnSystemsFinderMock> systemFinder;
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
    // At least one of systems is known as connected earlier - but the local is is the same.
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
    // We know about local system existance, which is offline right now.
    givenRecentSystem(kLocalId1);
    // Then user connects to the cloud and gets the same system info, including cloud id.
    givenCloudSystem({.cloudId = kCloudId1, .localId = kLocalId1, .online = false});
    thenSystemCountIs(1);
    whenLogoutFromCloud();

    // Recent system info should still be avialable.
    thenSystemCountIs(1);
}

}// namespace nx::vms::client::core::test
