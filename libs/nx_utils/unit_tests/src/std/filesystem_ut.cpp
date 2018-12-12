#include <gtest/gtest.h>

#include <nx/utils/std/filesystem.h>
#include <nx/utils/test_support/utils.h>

namespace std {
namespace filesystem {

TEST(StdFilesystem, parent_path)
{
    ASSERT_TRUE(path("/var/local").has_parent_path());
    ASSERT_FALSE(path("local").has_parent_path());
    ASSERT_TRUE(path("c:\\tmp").has_parent_path());
    ASSERT_FALSE(path("tmp").has_parent_path());

    ASSERT_EQ(path("/var"), path("/var/local").parent_path());
    ASSERT_EQ(path("/var/local"), path("/var/local/").parent_path());
    ASSERT_EQ(path("c:\\"), path("c:\\tmp").parent_path());
    ASSERT_EQ(path("c:/"), path("c:/tmp").parent_path());
    ASSERT_EQ(path(), path("tmp").parent_path());
}

TEST(StdFilesystem, filename)
{
    ASSERT_EQ(path("local"), path("/var/local").filename());
    ASSERT_EQ(path("hello.txt"), path("/var/local/hello.txt").filename());
    NX_GTEST_ASSERT_ANY_OF(
        ( path("."), path("") ), path("/var/local/").filename());
    ASSERT_EQ(path("tmp"), path("c:\\tmp").filename());
    ASSERT_EQ(path("tmp"), path("tmp").filename());
}

} // namespace filesystem
} // namespace std
