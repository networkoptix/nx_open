#include <gtest/gtest.h>
#include <vfs.h>
#include "test_common.h"

using namespace utils;

TEST(SimpleVfsFromJson, main) 
{
    VfsPair vfsPair;
    ASSERT_TRUE(buildVfsFromJson(kTestJson, "storage", &vfsPair));
    ASSERT_EQ(vfsPair.sampleFilePath, "/path/to/sample/file.mkv");

    auto vfsRoot = vfsPair.root;
    ASSERT_TRUE(vfsPair.root);

    /* check one node with details */
    auto node1 = FsStubNode_find(vfsRoot, "/storage/hi_quality/someCameraId1/2016/01/23/15/1453550461075.mkv");
    ASSERT_TRUE(node1);
    ASSERT_EQ(node1->type, file);
    ASSERT_EQ(node1->size, 429626247);

    /* check other nodes existence */
    FsStubNode_find(vfsRoot, "/storage/hi_quality/someCameraId1/2016/01/23/15/1453550461076.mkv");
    FsStubNode_find(vfsRoot, "/storage/low_quality/someCameraId1/2016/01/23/15/1453550461077.mkv");
    FsStubNode_find(vfsRoot, "/storage/hi_quality/someCameraId2/2016/01/23/15/1453550461078.mkv");
    FsStubNode_find(vfsRoot, "/storage/low_quality/someCameraId2/2016/01/23/15/1453550461079.mkv");
}