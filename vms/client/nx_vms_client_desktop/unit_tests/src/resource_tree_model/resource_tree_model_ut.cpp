#include "resource_tree_model_test_fixture.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource/user_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/avi/avi_resource.h>
#include <core/resource/file_layout_resource.h>

#include <ui/style/resource_icon_cache.h>

using namespace nx;
using namespace nx::vms::api;
using namespace nx::vms::client::desktop;

TEST_F(ResourceTreeModelTest, shouldShowPinnedNodesIfLoggedIn)
{
    // Define string constants.
    static constexpr auto systemName = "test_system";
    static constexpr auto userName = "test_user";

    // Define reference snapshot.
    ItemModelStateSnapshotHelper::SnapshotParams params;
    params.depth = 0;
    params.rowCount = 3;
    const auto referenceSnapshot = QJsonDocument::fromJson(QString(R"json(
        [
            {
                "data": {
                    "display": "%1",
                    "iconKey": "CurrentSystem"
                }
            },
            {
                "data": {
                    "display": "%2",
                    "iconKey": "User"
                }
            },
            {
                "data": {
                    "iconKey": "Unknown"
                }
            }
        ])json")
        .arg(systemName)
        .arg(userName)
        .toUtf8());

    // Perform actions.
    setSystemName(systemName);
    loginAsAdmin(userName);

    // Check result.
    ASSERT_TRUE(testSnapshot(params) == referenceSnapshot)
        << snapshotsOutputString(testSnapshot(params), referenceSnapshot);
}

TEST_F(ResourceTreeModelTest, singleServerShouldBeTopLevelNode)
{
    // Define string constants.
    static constexpr auto userName = "test_user";
    static constexpr auto serverName1 = "test_server_1";

    // Define reference snapshot.
    ItemModelStateSnapshotHelper::SnapshotParams params;
    params.depth = 1;
    params.startRow = 3;
    params.rowCount = 1;
    const auto referenceSnapshot = QJsonDocument::fromJson(QString(R"json(
        [
            {
                "data": {
                    "display": "%1",
                    "iconKey": "Server|Offline"
                }
            }
        ])json")
        .arg(serverName1)
        .toUtf8());

    // Perform actions.
    loginAsAdmin(userName);
    addServer(serverName1);

    // Check result.
    ASSERT_TRUE(testSnapshot(params) == referenceSnapshot)
        << snapshotsOutputString(testSnapshot(params), referenceSnapshot);
}

TEST_F(ResourceTreeModelTest, shouldGroupServersIfNotSingle)
{
    // Define string constants.
    static constexpr auto userName = "test_user";
    static constexpr auto serverName1 = "test_server_1";
    static constexpr auto serverName2 = "test_server_2";

    // Define reference snapshot.
    ItemModelStateSnapshotHelper::SnapshotParams params;
    params.depth = 1;
    params.startRow = 3;
    params.rowCount = 1;
    const auto referenceSnapshot = QJsonDocument::fromJson(QString(R"json(
        [
            {
                "data": {
                    "display": "Servers",
                    "iconKey": "Servers"
                },
                "children": [
                    {
                        "data": {
                            "display": "%1",
                            "iconKey": "Server|Offline"
                        }
                    },
                    {
                        "data": {
                            "display": "%2",
                            "iconKey": "Server|Offline"
                        }
                    }
                ]
            }
        ])json")
        .arg(serverName1)
        .arg(serverName2)
        .toUtf8());

    // Perform actions.
    loginAsAdmin(userName);
    addServer(serverName1);
    addServer(serverName2);

    // Check result.
    ASSERT_TRUE(testSnapshot(params) == referenceSnapshot)
        << snapshotsOutputString(testSnapshot(params), referenceSnapshot);
}

TEST_F(ResourceTreeModelTest, localFilesNodeVisibility)
{
    // Define string constants.
    static constexpr auto userName = "test_user";

    // Define reference data.
    const KeyValueVector lookupData =
        {{Qt::DisplayRole, "Local Files"},
        {Qn::ResourceIconKeyRole, QnResourceIconCache::LocalResources}};

    // Perform actions.

    // TODO: #vbreus On a first sight there should be a zero value, but actual representation of
    // the resource tree depends not only model state, but also on the root node set to the view.
    // Need to figure out how to achieve one to one correspondence of testing environment model and
    // actually displayed hierarchy.
    ASSERT_TRUE(getIndexByData(lookupData).size() == 1);
    loginAsAdmin(userName);
    ASSERT_TRUE(getIndexByData(lookupData).size() == 1);
    logout();
    // Same as above.
    ASSERT_TRUE(getIndexByData(lookupData).size() == 1);
}

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

TEST_F(ResourceTreeModelTest, offlineMediaResourcesAreNotDisplayed)
{
    // Define string constants.
    static constexpr auto userName = "test_user";
    static constexpr auto imageFilePath = "picture.bmp";
    static constexpr auto videoFilePath = "video.mov";
    static constexpr auto layoutFilePath = "layout.nov";

    // Define node lookup data.
    const KeyValueVector localFilesNodeLookupData =
    {{Qt::DisplayRole, "Local Files"},
    {Qn::ResourceIconKeyRole, QnResourceIconCache::LocalResources}};

    // Setup resources.
    loginAsAdmin(userName);
    addMediaResource(imageFilePath)->setStatus(Qn::Offline);
    addMediaResource(videoFilePath)->setStatus(Qn::Offline);
    addMediaResource(layoutFilePath)->setStatus(Qn::Offline);

    // Define reference snapshot.
    ItemModelStateSnapshotHelper::SnapshotParams params;
    params.parentIndex = getIndexByData(localFilesNodeLookupData).first();
    const auto referenceSnapshot = QJsonDocument::fromJson(QString(R"json(
        [
        ])json")
        .toUtf8());

    // Check result.
    ASSERT_TRUE(testSnapshot(params) == referenceSnapshot)
        << snapshotsOutputString(testSnapshot(params), referenceSnapshot);
}

TEST_F(ResourceTreeModelTest, localResourceVisibilityTransitions)
{
    // Define string constants.
    static constexpr auto userName = "test_user";
    static constexpr auto resourceFilePath = "test_resource.mkv";

    // Define reference data.
    const KeyValueVector lookupData = { {Qt::DisplayRole, resourceFilePath} };

    // Perform actions.
    const auto mediaResource = addMediaResource(resourceFilePath);
    mediaResource->setStatus(Qn::Online);
    ASSERT_TRUE(getIndexByData(lookupData).size() == 1);

    mediaResource->setStatus(Qn::Offline);
    ASSERT_TRUE(getIndexByData(lookupData).size() == 0);

    mediaResource->setStatus(Qn::Online);
    ASSERT_TRUE(getIndexByData(lookupData).size() == 1);
}

TEST_F(ResourceTreeModelTest, localResourcesIconsAndGrouping)
{
    // Define string constants.
    static constexpr auto userName = "test_user";
    static constexpr auto imageFilePath = "1_picture.png";
    static constexpr auto videoFilePath = "2_video.avi";
    static constexpr auto layoutFilePath = "3_layout.nov";
    static constexpr auto encryptedLayoutFilePath = "4_encrypted_layout.nov";

    // Define node lookup data.
    const KeyValueVector localFilesNodeLookupData =
    {{Qt::DisplayRole, "Local Files"},
    {Qn::ResourceIconKeyRole, QnResourceIconCache::LocalResources}};

    // Setup resources.
    loginAsAdmin(userName);
    addMediaResource(imageFilePath)->setStatus(Qn::Online);
    addMediaResource(videoFilePath)->setStatus(Qn::Online);
    addFileLayoutResource(layoutFilePath)->setStatus(Qn::Online);
    addFileLayoutResource(encryptedLayoutFilePath,/*isEncrypted*/ true)->setStatus(Qn::Online);

    // Define reference snapshot.
    ItemModelStateSnapshotHelper::SnapshotParams params;
    params.parentIndex = getIndexByData(localFilesNodeLookupData).first();
    params.depth = 1;
    const auto referenceSnapshot = QJsonDocument::fromJson(QString(R"json(
        [
            {
                "data": {
                    "display": "3_layout.nov",
                    "iconKey": "ExportedLayout"
                }
            },
            {
                "data": {
                    "display": "4_encrypted_layout.nov",
                    "iconKey": "ExportedEncryptedLayout"
                }
            },
            {
                "data": {
                    "display": "2_video.avi",
                    "iconKey": "Media|Online"
                }
            },
            {
                "data": {
                    "display": "1_picture.png",
                    "iconKey": "Image|Online"
                }
            }
        ])json")
        .toUtf8());

    // Check result.
    ASSERT_TRUE(testSnapshot(params) == referenceSnapshot)
        << snapshotsOutputString(testSnapshot(params), referenceSnapshot);
}

TEST_F(ResourceTreeModelTest, mediaResourceOnlyFilenameDisplayed)
{
    // Define string constants.
    static constexpr auto userName = "test_user";
    static constexpr auto imageFilePath = "test_directory/picture.png";

    // Define node lookup data.
    const KeyValueVector localFilesNodeLookupData =
    {{Qt::DisplayRole, "Local Files"},
    {Qn::ResourceIconKeyRole, QnResourceIconCache::LocalResources}};

    // Setup resources.
    loginAsAdmin(userName);
    addMediaResource(imageFilePath)->setStatus(Qn::Online);

    // Define reference snapshot.
    ItemModelStateSnapshotHelper::SnapshotParams params;
    params.parentIndex = getIndexByData(localFilesNodeLookupData).first();
    const auto referenceSnapshot = QJsonDocument::fromJson(QString(R"json(
        [
            {
                "data": {
                    "display": "picture.png",
                    "iconKey": "Image|Online"
                }
            }
        ])json")
        .toUtf8());

    // Check result.
    ASSERT_TRUE(testSnapshot(params) == referenceSnapshot)
        << snapshotsOutputString(testSnapshot(params), referenceSnapshot);
}
