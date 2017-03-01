#include <vfs.h>

TEST(SimpleVfsFromJson, main) 
{
    const char* testJson = R"JSON(
"sample": "/tmp/sample.mkv",
"chunks": [{"durationMs":429626247,"startTimeMs":1453550461075},{"durationMs":439532418,"startTimeMs":1453993336069}]
)JSON";
}