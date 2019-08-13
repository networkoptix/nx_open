#include "resource_tree_model_test_fixture.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource/user_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/avi/avi_resource.h>
#include <core/resource/file_layout_resource.h>
#include <core/resource/webpage_resource.h>

#include <ui/style/resource_icon_cache.h>

using namespace nx;
using namespace nx::vms::api;
using namespace nx::vms::client::desktop;

// TODO: #vbreus Bunch of almost identical tests cases here this time. Make them parameterized
// rather than copy-pasted.

TEST_F(ResourceTreeModelTest, localFilesAreSortedAlphanumerically)
{
    // Define string constants.
    static constexpr auto userName = "test_user";
    static constexpr auto filePath1 = "picture1_2.png";
    static constexpr auto filePath2 = "picture2_11.png";
    static constexpr auto filePath3 = "picture10_1.png";

    // Define node lookup data.
    const KeyValueVector localFilesNodeLookupData =
        {{Qt::DisplayRole, "Local Files"},
        {Qn::ResourceIconKeyRole, QnResourceIconCache::LocalResources}};

    // Setup resources.
    loginAsAdmin(userName);
    addMediaResource(filePath1)->setStatus(Qn::Online);
    addMediaResource(filePath2)->setStatus(Qn::Online);
    addMediaResource(filePath3)->setStatus(Qn::Online);

    // Define reference snapshot.
    ItemModelStateSnapshotHelper::SnapshotParams params;
    params.parentIndex = getIndexByData(localFilesNodeLookupData).first();
    const auto referenceSnapshot = QJsonDocument::fromJson(QString(R"json(
        [
            {
                "data": {
                    "display": "picture1_2.png",
                    "iconKey": "Image|Online"
                }
            },
            {
                "data": {
                    "display": "picture2_11.png",
                    "iconKey": "Image|Online"
                }
            },
            {
                "data": {
                    "display": "picture10_1.png",
                    "iconKey": "Image|Online"
                }
            }
        ])json")
        .toUtf8());

    // Check result.
    ASSERT_TRUE(testSnapshot(params) == referenceSnapshot)
        << snapshotsOutputString(testSnapshot(params), referenceSnapshot);
}

TEST_F(ResourceTreeModelTest, localFilesAreSortedCaseInsensitive)
{
    // Define string constants.
    static constexpr auto userName = "test_user";
    static constexpr auto filePath1 = "A.png";
    static constexpr auto filePath2 = "b.png";
    static constexpr auto filePath3 = "C.png";

    // Define node lookup data.
    const KeyValueVector localFilesNodeLookupData =
        {{Qt::DisplayRole, "Local Files"},
        {Qn::ResourceIconKeyRole, QnResourceIconCache::LocalResources}};

    // Setup resources.
    loginAsAdmin(userName);
    addMediaResource(filePath1)->setStatus(Qn::Online);
    addMediaResource(filePath2)->setStatus(Qn::Online);
    addMediaResource(filePath3)->setStatus(Qn::Online);

    // Define reference snapshot.
    ItemModelStateSnapshotHelper::SnapshotParams params;
    params.parentIndex = getIndexByData(localFilesNodeLookupData).first();
    const auto referenceSnapshot = QJsonDocument::fromJson(QString(R"json(
        [
            {
                "data": {
                    "display": "A.png",
                    "iconKey": "Image|Online"
                }
            },
            {
                "data": {
                    "display": "b.png",
                    "iconKey": "Image|Online"
                }
            },
            {
                "data": {
                    "display": "C.png",
                    "iconKey": "Image|Online"
                }
            }
        ])json")
        .toUtf8());

    // Check result.
    ASSERT_TRUE(testSnapshot(params) == referenceSnapshot)
        << snapshotsOutputString(testSnapshot(params), referenceSnapshot);
}

TEST_F(ResourceTreeModelTest, webPagesAreSortedAlphanumerically)
{
    // Define string constants.
    static constexpr auto userName = "test_user";
    static constexpr auto webPageName1 = "web_page_1_2";
    static constexpr auto webPageName2 = "web_page_2_11";
    static constexpr auto webPageName3 = "web_page_10_1";

    // Define node lookup data.
    const KeyValueVector webPagesNodeLookupData =
        {{Qt::DisplayRole, "Web Pages"},
        {Qn::ResourceIconKeyRole, QnResourceIconCache::WebPages}};

    // Setup resources.
    loginAsAdmin(userName);
    addWebPageResource(webPageName1)->setStatus(Qn::Online);
    addWebPageResource(webPageName2)->setStatus(Qn::Online);
    addWebPageResource(webPageName3)->setStatus(Qn::Online);

    // Define reference snapshot.
    ItemModelStateSnapshotHelper::SnapshotParams params;
    params.parentIndex = getIndexByData(webPagesNodeLookupData).first();
    const auto referenceSnapshot = QJsonDocument::fromJson(QString(R"json(
        [
            {
                "data": {
                    "display": "web_page_1_2",
                    "iconKey": "WebPage|Online"
                }
            },
            {
                "data": {
                    "display": "web_page_2_11",
                    "iconKey": "WebPage|Online"
                }
            },
            {
                "data": {
                    "display": "web_page_10_1",
                    "iconKey": "WebPage|Online"
                }
            }
        ])json")
        .toUtf8());

    // Check result.
    ASSERT_TRUE(testSnapshot(params) == referenceSnapshot)
        << snapshotsOutputString(testSnapshot(params), referenceSnapshot);
}

TEST_F(ResourceTreeModelTest, webPagesAreSortedCaseInsensitive)
{
    // Define string constants.
    static constexpr auto userName = "test_user";
    static constexpr auto webPageName1 = "A";
    static constexpr auto webPageName2 = "b";
    static constexpr auto webPageName3 = "C";

    // Define node lookup data.
    const KeyValueVector webPagesNodeLookupData =
        {{Qt::DisplayRole, "Web Pages"},
        {Qn::ResourceIconKeyRole, QnResourceIconCache::WebPages}};

    // Setup resources.
    loginAsAdmin(userName);
    addWebPageResource(webPageName1)->setStatus(Qn::Online);
    addWebPageResource(webPageName2)->setStatus(Qn::Online);
    addWebPageResource(webPageName3)->setStatus(Qn::Online);

    // Define reference snapshot.
    ItemModelStateSnapshotHelper::SnapshotParams params;
    params.parentIndex = getIndexByData(webPagesNodeLookupData).first();
    const auto referenceSnapshot = QJsonDocument::fromJson(QString(R"json(
        [
            {
                "data": {
                    "display": "A",
                    "iconKey": "WebPage|Online"
                }
            },
            {
                "data": {
                    "display": "b",
                    "iconKey": "WebPage|Online"
                }
            },
            {
                "data": {
                    "display": "C",
                    "iconKey": "WebPage|Online"
                }
            }
        ])json")
        .toUtf8());

    // Check result.
    ASSERT_TRUE(testSnapshot(params) == referenceSnapshot)
        << snapshotsOutputString(testSnapshot(params), referenceSnapshot);
}

TEST_F(ResourceTreeModelTest, layoutsAreSortedAlphanumerically)
{
    // Define string constants.
    static constexpr auto userName = "test_user";
    static constexpr auto layoutName1 = "layout_1_2";
    static constexpr auto layoutName2 = "layout_2_11";
    static constexpr auto layoutName3 = "layout_10_1";

    // Define node lookup data.
    const KeyValueVector webPagesNodeLookupData =
        {{Qt::DisplayRole, "Layouts"},
        {Qn::ResourceIconKeyRole, QnResourceIconCache::Layouts}};

    // Setup resources.
    auto user = loginAsAdmin(userName);
    addLayoutResource(layoutName1, user->getId());
    addLayoutResource(layoutName2, user->getId());
    addLayoutResource(layoutName3, user->getId());

    // Define reference snapshot.
    ItemModelStateSnapshotHelper::SnapshotParams params;
    params.parentIndex = getIndexByData(webPagesNodeLookupData).first();
    const auto referenceSnapshot = QJsonDocument::fromJson(QString(R"json(
        [
            {
                "data": {
                    "display": "layout_1_2",
                    "iconKey": "Layout"
                }
            },
            {
                "data": {
                    "display": "layout_2_11",
                    "iconKey": "Layout"
                }
            },
            {
                "data": {
                    "display": "layout_10_1",
                    "iconKey": "Layout"
                }
            }
        ])json")
        .toUtf8());

    // Check result.
    ASSERT_TRUE(testSnapshot(params) == referenceSnapshot)
        << snapshotsOutputString(testSnapshot(params), referenceSnapshot);
}

TEST_F(ResourceTreeModelTest, layoutsAreSortedCaseInsensitive)
{
    // Define string constants.
    static constexpr auto userName = "test_user";
    static constexpr auto layoutName1 = "A";
    static constexpr auto layoutName2 = "b";
    static constexpr auto layoutName3 = "C";

    // Define node lookup data.
    const KeyValueVector webPagesNodeLookupData =
        {{Qt::DisplayRole, "Layouts"},
        {Qn::ResourceIconKeyRole, QnResourceIconCache::Layouts}};

    // Setup resources.
    auto user = loginAsAdmin(userName);
    addLayoutResource(layoutName1, user->getId());
    addLayoutResource(layoutName2, user->getId());
    addLayoutResource(layoutName3, user->getId());

    // Define reference snapshot.
    ItemModelStateSnapshotHelper::SnapshotParams params;
    params.parentIndex = getIndexByData(webPagesNodeLookupData).first();
    const auto referenceSnapshot = QJsonDocument::fromJson(QString(R"json(
        [
            {
                "data": {
                    "display": "A",
                    "iconKey": "Layout"
                }
            },
            {
                "data": {
                    "display": "b",
                    "iconKey": "Layout"
                }
            },
            {
                "data": {
                    "display": "C",
                    "iconKey": "Layout"
                }
            }
        ])json")
        .toUtf8());

    // Check result.
    ASSERT_TRUE(testSnapshot(params) == referenceSnapshot)
        << snapshotsOutputString(testSnapshot(params), referenceSnapshot);
}
