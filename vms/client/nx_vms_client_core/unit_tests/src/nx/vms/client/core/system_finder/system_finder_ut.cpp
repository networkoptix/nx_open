// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <QtCore/QCoreApplication>

#include <network/system_helpers.h>
#include <nx/branding.h>
#include <nx/vms/client/core/application_context.h>
#include <nx/vms/client/core/settings/client_core_settings.h>
#include <nx/vms/client/core/system_finder/private/cloud_system_description.h>
#include <nx/vms/client/core/system_finder/private/local_system_description.h>
#include <nx/vms/client/core/system_finder/system_finder.h>
#include <nx/vms/common/network/server_compatibility_validator.h>

#include "system_finder_mock.h"

namespace nx::vms::client::core::test {

namespace {

const QString kCloudId1 = nx::Uuid::createUuid().toString();
const QString kCloudId2 = nx::Uuid::createUuid().toString();
const nx::Uuid kLocalId1 = nx::Uuid::createUuid();
const nx::Uuid kLocalId2 = nx::Uuid::createUuid();

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
        QCoreApplication::setOrganizationName(nx::branding::company());
        QCoreApplication::setApplicationName("Unit tests");
        appContext = std::make_unique<ApplicationContext>(
            ApplicationContext::Mode::unitTests,
            nx::vms::api::PeerType::desktopClient,
            /*customCloudHost*/ QString(),
            /*ignoreCustomization*/ false);
        systemFinder = std::make_unique<QnSystemFinderMock>();
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
        const nx::Uuid localId = helpers::getLocalSystemId(system);
        systemFinder->directFinder->addSystem(
            LocalSystemDescription::create(systemId, localId, "Direct Accessible System"));
    }

    /** Have a record about a system we connected recenlty (only local id is stored). */
    void givenRecentSystem(nx::Uuid localId)
    {
        systemFinder->recentFinder->addSystem(
            LocalSystemDescription::create(localId.toSimpleString(), localId, "Recent System"));
    }

    void thenSystemCountIs(int count)
    {
        ASSERT_EQ(systemFinder->systems().count(), count);
    }

    void thenSystemIsOnline(const QString& id, bool value = true)
    {
        ASSERT_EQ(systemFinder->getSystem(id)->isOnline(), value);
    }

public:
    std::unique_ptr<ApplicationContext> appContext;
    std::unique_ptr<QnSystemFinderMock> systemFinder;
};

TEST_F(SystemFinderTest, noSystemsByDefault)
{
    thenSystemCountIs(0);
}

// Behavior is not defined yet.
TEST_F(SystemFinderTest, DISABLED_offlineCloudSystemIsMergedToOnline)
{
    givenCloudSystem({.cloudId = kCloudId1, .localId = kLocalId1, .online = true});
    givenCloudSystem({.cloudId = kCloudId2, .localId = kLocalId1, .online = false});

    thenSystemCountIs(1);
    thenSystemIsOnline(kCloudId1);
}

// VMS-53593: Multiple online cloud systems with the same local id should not be merged.
TEST_F(SystemFinderTest, manyCloudSystemsWithSameLocalIdAreDisplayedSeparately)
{
    givenCloudSystem({.cloudId = kCloudId1, .localId = kLocalId1, .online = true});
    givenCloudSystem({.cloudId = kCloudId2, .localId = kLocalId1, .online = true});
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
    // And then saved connectio to this system.
    givenRecentSystem(kLocalId1);
    // Then systems should be merged.
    thenSystemCountIs(1);
}

}// namespace nx::vms::client::core::test
