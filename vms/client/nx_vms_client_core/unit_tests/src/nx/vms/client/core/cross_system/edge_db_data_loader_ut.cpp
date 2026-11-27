// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <algorithm>
#include <functional>
#include <vector>

#include <gtest/gtest.h>

#include <nx/vms/client/core/application_context.h>
#include <nx/vms/client/core/cross_system/edge_db_data_loader.h>
#include <nx/vms/client/core/network/edge_db_connection.h>

#include "test_edge_db_connection.h"

namespace nx::vms::client::core {
namespace test {

using namespace nx::vms::api;
namespace StatusCode = nx::network::http::StatusCode;

namespace {

const nx::Uuid kOrganizationId("6c4b1a5e-0f43-4b8e-9a43-3f7d5e2b9a01");
const nx::Uuid kOtherOrganizationId("0d9b2c7a-5e1f-4a6b-8c3d-2e4f6a8b0c12");
const QString kSiteId = "1f6e2b9c-7d4a-4c3e-8b5a-9e0d1c2b3a4f";
const QString kOtherSiteId = "2a7f3c0d-8e5b-4d4f-9c6b-0f1e2d3c4b5a";
const QString kCustomGroupIdProperty = "customGroupId";
const nx::Uuid kUserId("8e1f0a2b-3c4d-4e5f-a617-28394a5b6c7d");
const nx::Uuid kDeviceTypeId("5d2e7f10-4a6b-4c8d-9e0f-1a2b3c4d5e6f");

WithSite<DeviceModelV4> device(int index)
{
    WithSite<DeviceModelV4> result;
    result.id = nx::Uuid::fromArbitraryData(QString("device %1").arg(index));
    result.name = QString("Camera %1").arg(index);
    result.siteId = kSiteId;
    result.typeId = kDeviceTypeId;
    result.status = ResourceStatus::online;
    result.parameters[kCustomGroupIdProperty] = QJsonValue(QString("Group %1").arg(index));
    result.parameters["streamUrls"] = QJsonValue("Not requested");
    result.group = DeviceGroupSettings{
        .id = QString("recorder %1").arg(index), .name = QString("Recorder %1").arg(index)};
    return result;
}

WithSite<LayoutData> layout(int index)
{
    WithSite<LayoutData> result;
    result.id = nx::Uuid::fromArbitraryData(QString("layout %1").arg(index));
    result.name = QString("Layout %1").arg(index);
    result.siteId = kSiteId;
    result.parentId = kUserId;
    result.cellSpacing = 0.1f;

    LayoutItemData item;
    item.id = nx::Uuid::fromArbitraryData(QString("layout item %1").arg(index));
    item.resourceId = device(index).id;
    result.items.push_back(item);
    return result;
}

} // namespace

class EdgeDbDataLoaderTest: public ::testing::Test
{
protected:
    EdgeDbDataLoaderTest()
    {
        QObject::connect(&loader, &EdgeDbDataLoader::ready, [this]() { ++readyCount; });
        QObject::connect(&loader,
            &EdgeDbDataLoader::failed,
            [this](const nx::network::rest::Result& error) { errors.push_back(error); });

        connection.handler = edgeDb.handler();
    }

    void givenOrganization(
        const nx::Uuid& organizationId, int deviceCount = 3, int layoutCount = 3)
    {
        edgeDb = FakeEdgeDb{.organizationId = organizationId};
        edgeDb.permissions[kSiteId] = PermissionsModel();
        for (int i = 0; i < deviceCount; ++i)
            edgeDb.add("devices", device(i));
        for (int i = 0; i < layoutCount; ++i)
            edgeDb.add("layouts", layout(i));
    }

    void givenResponse(const QString& collection,
        TestEdgeDbConnection::Reply response,
        const QString& cursor = {})
    {
        connection.responses[{edgeDbPath(kOrganizationId, collection), cursor}] = response;
    }

    void whenLoadingFinishes()
    {
        waitFor([this]() { return readyCount > 0 || !errors.empty(); });
    }

    void thenLoaded() const
    {
        ASSERT_TRUE(errors.empty()) << errors[0].errorString.toStdString();
        ASSERT_EQ(readyCount, 1);
        ASSERT_TRUE(loader.isReady());
    }

    void thenFailedOnce() const
    {
        EXPECT_EQ(readyCount, 0);
        ASSERT_EQ(errors.size(), 1u);
        EXPECT_FALSE(loader.isReady());
        EXPECT_TRUE(loader.permissions().empty());
        EXPECT_TRUE(loader.sites().empty());
    }

    /** Loaded data of the only Site that test Organizations have by default. */
    const EdgeDbDataLoader::SiteData& site() const { return loader.sites().at(kSiteId); }

    size_t requestCount(const QString& path) const
    {
        return std::count_if(connection.requests.begin(),
            connection.requests.end(),
            [&path](const auto& request) { return request.path == path; });
    }

protected:
    ApplicationContext appContext{
        ApplicationContext::Mode::unitTests, Qn::SerializationFormat::json};
    FakeEdgeDb edgeDb;
    TestEdgeDbConnection connection;
    EdgeDbDataLoader loader{&connection};
    int readyCount = 0;
    std::vector<nx::network::rest::Result> errors;
};

TEST_F(EdgeDbDataLoaderTest, loadsAllCollectionsOfOrganization)
{
    givenOrganization(kOrganizationId);

    loader.start(kOrganizationId);
    whenLoadingFinishes();
    thenLoaded();

    const auto& permissions = loader.permissions();
    ASSERT_EQ(permissions.size(), 1u);
    EXPECT_TRUE(permissions.contains(kSiteId));

    ASSERT_EQ(loader.sites().size(), 1u);
    ASSERT_TRUE(loader.sites().contains(kSiteId));

    auto devices = site().devices;
    ASSERT_EQ(devices.size(), 3u);
    std::ranges::sort(devices, {}, &DeviceModelV4::name);
    for (int i = 0; i < 3; ++i)
    {
        const auto expected = device(i);
        EXPECT_EQ(devices[i].id, expected.id);
        EXPECT_EQ(devices[i].siteId, kSiteId);
        EXPECT_EQ(devices[i].name, expected.name);
        EXPECT_EQ(devices[i].typeId, kDeviceTypeId);
        EXPECT_EQ(devices[i].group, expected.group);

        // Not requested.
        EXPECT_FALSE(devices[i].status);

        // Only the custom group is requested from the parameters.
        ASSERT_EQ(devices[i].parameters.size(), 1u);
        EXPECT_EQ(devices[i].parameters[kCustomGroupIdProperty],
            expected.parameters.at(kCustomGroupIdProperty));
    }

    auto layouts = site().layouts;
    ASSERT_EQ(layouts.size(), 3u);
    std::ranges::sort(layouts, {}, &LayoutData::name);
    for (int i = 0; i < 3; ++i)
    {
        const auto expected = layout(i);
        EXPECT_EQ(layouts[i].id, expected.id);
        EXPECT_EQ(layouts[i].siteId, kSiteId);
        EXPECT_EQ(layouts[i].name, expected.name);
        EXPECT_EQ(layouts[i].parentId, kUserId);

        // Not requested.
        EXPECT_TRUE(layouts[i].items.empty());
        EXPECT_FLOAT_EQ(layouts[i].cellSpacing, LayoutData().cellSpacing);
    }
}

TEST_F(EdgeDbDataLoaderTest, recordsAreGroupedBySites)
{
    givenOrganization(kOrganizationId, /*deviceCount*/ 2, /*layoutCount*/ 0);
    auto otherSiteDevice = device(2);
    otherSiteDevice.siteId = kOtherSiteId;
    auto otherSiteLayout = layout(2);
    otherSiteLayout.siteId = kOtherSiteId;
    edgeDb.add("devices", otherSiteDevice);
    edgeDb.add("layouts", otherSiteLayout);

    loader.start(kOrganizationId);
    whenLoadingFinishes();
    thenLoaded();

    ASSERT_EQ(loader.sites().size(), 2u);
    EXPECT_EQ(site().devices.size(), 2u);
    EXPECT_TRUE(site().layouts.empty());

    const auto& otherSite = loader.sites().at(kOtherSiteId);
    ASSERT_EQ(otherSite.devices.size(), 1u);
    EXPECT_EQ(otherSite.devices[0].id, otherSiteDevice.id);
    ASSERT_EQ(otherSite.layouts.size(), 1u);
    EXPECT_EQ(otherSite.layouts[0].id, otherSiteLayout.id);
}

TEST_F(EdgeDbDataLoaderTest, readsAllListPages)
{
    givenOrganization(kOrganizationId, /*deviceCount*/ 150, /*layoutCount*/ 150);
    edgeDb.pageSize = 100;

    loader.start(kOrganizationId);
    whenLoadingFinishes();
    thenLoaded();

    EXPECT_EQ(site().devices.size(), 150u);
    EXPECT_EQ(site().layouts.size(), 150u);
    EXPECT_EQ(requestCount(edgeDbPath(kOrganizationId, "devices")), 2u);
    EXPECT_EQ(requestCount(edgeDbPath(kOrganizationId, "layouts")), 2u);
    EXPECT_EQ(connection.requests.size(), 5u); //< With the permissions.

    for (const auto& request: connection.requests)
        EXPECT_FALSE(request.params.contains("organizationId"));
}

TEST_F(EdgeDbDataLoaderTest, emptyOrganizationIsLoaded)
{
    givenOrganization(kOrganizationId, /*deviceCount*/ 0, /*layoutCount*/ 0);

    loader.start(kOrganizationId);
    whenLoadingFinishes();
    thenLoaded();

    EXPECT_TRUE(loader.sites().empty());
}

TEST_F(EdgeDbDataLoaderTest, errorOnNextPageDiscardsAllData)
{
    givenOrganization(kOrganizationId);
    givenResponse("devices", edgeDbResponse(StatusCode::internalServerError, {}), "2");

    loader.start(kOrganizationId);
    whenLoadingFinishes();
    thenFailedOnce();
}

TEST_F(EdgeDbDataLoaderTest, errorOfLayoutsDiscardsAllData)
{
    givenOrganization(kOrganizationId);
    givenResponse("layouts", edgeDbError(StatusCode::forbidden, "forbidden", "No access"));

    loader.start(kOrganizationId);
    whenLoadingFinishes();
    thenFailedOnce();

    EXPECT_EQ(errors[0].errorId, nx::network::rest::ErrorId::forbidden);
    EXPECT_EQ(errors[0].errorString, "No access");
}

TEST_F(EdgeDbDataLoaderTest, serviceErrorIsReported)
{
    givenOrganization(kOrganizationId);
    givenResponse("devices",
        edgeDbError(StatusCode::badRequest, "invalidParameter", "Field cannot be requested"));

    loader.start(kOrganizationId);
    whenLoadingFinishes();
    thenFailedOnce();

    EXPECT_EQ(errors[0].errorId, nx::network::rest::ErrorId::invalidParameter);
    EXPECT_EQ(errors[0].errorString, "Field cannot be requested");
}

TEST_F(EdgeDbDataLoaderTest, errorWithoutBodyIsReported)
{
    givenOrganization(kOrganizationId);
    givenResponse("devices", edgeDbResponse(StatusCode::notFound, {}));

    loader.start(kOrganizationId);
    whenLoadingFinishes();
    thenFailedOnce();

    EXPECT_EQ(errors[0].errorId, nx::network::rest::ErrorId::notFound);
    EXPECT_FALSE(errors[0].errorString.isEmpty());
}

TEST_F(EdgeDbDataLoaderTest, errorOfAnyCollectionIsReportedOnce)
{
    givenOrganization(kOrganizationId);
    givenResponse("permissions", std::unexpected(SystemError::connectionReset));
    givenResponse("layouts", edgeDbResponse(StatusCode::internalServerError, {}));

    loader.start(kOrganizationId);
    whenLoadingFinishes();
    thenFailedOnce();

    EXPECT_EQ(errors[0].errorId, nx::network::rest::ErrorId::serviceUnavailable);
}

TEST_F(EdgeDbDataLoaderTest, networkErrorIsReported)
{
    givenOrganization(kOrganizationId);
    givenResponse("devices", std::unexpected(SystemError::connectionReset));

    loader.start(kOrganizationId);
    whenLoadingFinishes();
    thenFailedOnce();

    EXPECT_EQ(errors[0].errorId, nx::network::rest::ErrorId::serviceUnavailable);
}

TEST_F(EdgeDbDataLoaderTest, restartDiscardsResponsesOfPreviousRun)
{
    givenOrganization(kOtherOrganizationId);
    givenResponse("layouts", edgeDbResponse(StatusCode::internalServerError, {}));
    connection.deferReplies = true;

    loader.start(kOrganizationId);
    loader.start(kOtherOrganizationId);

    // Replies of both runs arrive, the failing one of the first run must be ignored.
    connection.deferReplies = false;
    connection.replyAll();
    whenLoadingFinishes();
    thenLoaded();

    EXPECT_EQ(site().devices.size(), 3u);
    EXPECT_EQ(site().layouts.size(), 3u);
}

TEST_F(EdgeDbDataLoaderTest, restartCancelsRequestsOfPreviousRun)
{
    givenOrganization(kOrganizationId);
    connection.deferReplies = true;

    loader.start(kOrganizationId);
    const auto firstRunRequests = connection.requests.size();
    loader.start(kOrganizationId);

    EXPECT_EQ(connection.canceled.size(), firstRunRequests);
    EXPECT_TRUE(std::ranges::is_sorted(connection.canceled, std::greater{}));

    connection.deferReplies = false;
    connection.replyAll();
    whenLoadingFinishes();
    thenLoaded();
}

TEST_F(EdgeDbDataLoaderTest, failureCancelsRunningRequests)
{
    givenOrganization(kOrganizationId);
    givenResponse("permissions", edgeDbResponse(StatusCode::internalServerError, {}));

    loader.start(kOrganizationId);
    whenLoadingFinishes();
    thenFailedOnce();

    // Devices and layouts lists were running when permissions failed.
    EXPECT_EQ(connection.canceled.size(), 2u);
}

TEST_F(EdgeDbDataLoaderTest, staleResponseAfterReadyIsIgnored)
{
    // Requests of the first run are not known to edgeDb and would fail if not ignored.
    givenOrganization(kOrganizationId);
    connection.deferReplies = true;
    loader.start(kOtherOrganizationId);

    connection.deferReplies = false;
    loader.start(kOrganizationId);
    whenLoadingFinishes();
    thenLoaded();

    // Stale replies are delivered through the event queue.
    connection.replyAll();
    QCoreApplication::processEvents();

    EXPECT_EQ(readyCount, 1);
    EXPECT_TRUE(errors.empty());
    EXPECT_TRUE(loader.isReady());
    EXPECT_EQ(site().devices.size(), 3u);
}

TEST_F(EdgeDbDataLoaderTest, reloadAfterFailureSucceeds)
{
    givenOrganization(kOrganizationId);
    givenResponse("layouts", edgeDbResponse(StatusCode::internalServerError, {}));

    loader.start(kOrganizationId);
    whenLoadingFinishes();
    ASSERT_EQ(errors.size(), 1u);

    connection.responses.clear();
    loader.start(kOrganizationId);
    waitFor([this]() { return readyCount > 0; });

    EXPECT_EQ(readyCount, 1);
    EXPECT_EQ(errors.size(), 1u);
    EXPECT_TRUE(loader.isReady());
}

/**
 * Without the Cloud connection the real connection does not send the requests, and the loader must
 * report one error instead of hanging.
 */
TEST(EdgeDbConnectionTest, requestWhenCloudIsOfflineFails)
{
    ApplicationContext appContext{
        ApplicationContext::Mode::unitTests, Qn::SerializationFormat::json};
    EdgeDbConnection connection;
    EdgeDbDataLoader loader{&connection};

    std::vector<nx::network::rest::Result> errors;
    QObject::connect(&loader,
        &EdgeDbDataLoader::failed,
        [&errors](const nx::network::rest::Result& error) { errors.push_back(error); });

    loader.start(kOrganizationId);
    waitFor([&errors]() { return !errors.empty(); });

    ASSERT_EQ(errors.size(), 1u);
    EXPECT_EQ(errors[0].errorId, nx::network::rest::ErrorId::serviceUnavailable);
    EXPECT_FALSE(loader.isReady());
}

} // namespace test
} // namespace nx::vms::client::core
