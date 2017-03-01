#include <gtest/gtest.h>
#include <vfs.h>

using namespace utils;

TEST(SimpleVfsFromJson, main) 
{
    /**
    1453550461075 - Sat Jan 23 2016 15:01:01
    */
    const char* testJson = R"JSON(
{
    "sample": "/path/to/sample/file.mkv",
    "cameras": [
        {
            "id": "someCameraId1",
            "hi": [
                {
                    "durationMs": "429626247",
                    "startTimeMs": "1453550461075"
                },
                {
                    "durationMs": "429626247",
                    "startTimeMs": "1453550461076"
                }
            ],
            "low": [
                {
                    "durationMs": "429626247",
                    "startTimeMs": "1453550461077"
                } 
            ]
        }
        ,
        {
            "id": "someCameraId2",
            "hi": [
                {
                    "durationMs": "429626247",
                    "startTimeMs": "1453550461078"
                }
            ],
            "low": [
                {
                    "durationMs": "429626247",
                    "startTimeMs": "1453550461079"
                } 
            ]
        }        
    ]
}
)JSON";

    VfsPair vfsPair;
    ASSERT_TRUE(buildVfsFromJson(testJson, "storage", &vfsPair));
    ASSERT_EQ(vfsPair.sampleFilePath, "/path/to/sample/file.mkv");

    auto vfsRoot = vfsPair.root;
    ASSERT_TRUE(vfsPair.root);

    /* check one node with details */
    auto node1 = FsStubNode_find(vfsRoot, "/storage/hi_quality/someCameraId1/2016/1/23/15/1453550461075.mkv");
    ASSERT_TRUE(node1);
    ASSERT_EQ(node1->type, file);
    ASSERT_EQ(node1->size, 429626247);

    /* check other nodes existence */
    FsStubNode_find(vfsRoot, "/storage/hi_quality/someCameraId1/2016/1/23/15/1453550461076.mkv");
    FsStubNode_find(vfsRoot, "/storage/low_quality/someCameraId1/2016/1/23/15/1453550461077.mkv");
    FsStubNode_find(vfsRoot, "/storage/hi_quality/someCameraId2/2016/1/23/15/1453550461078.mkv");
    FsStubNode_find(vfsRoot, "/storage/low_quality/someCameraId2/2016/1/23/15/1453550461079.mkv");
}