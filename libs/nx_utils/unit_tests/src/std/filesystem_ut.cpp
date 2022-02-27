// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/utils/std/filesystem.h>
#include <nx/utils/test_support/utils.h>

namespace nx::utils::filesystem {

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

    ASSERT_EQ("bar.txt", path("/foo/bar.txt").filename());
    ASSERT_EQ(".bar", path("/foo/.bar").filename());
    ASSERT_EQ("", path("/foo/bar/").filename());
    ASSERT_EQ(".", path("/foo/.").filename());
    ASSERT_EQ("..", path("/foo/..").filename());
    ASSERT_EQ(".", path(".").filename());
    ASSERT_EQ("..", path("..").filename());
    ASSERT_EQ("", path("/").filename());
    //ASSERT_EQ("host", path("//host").filename());
}

TEST(StdFilesystem, extension)
{
    ASSERT_EQ(".txt", path("/foo/bar.txt").extension());
    ASSERT_EQ("", path("/foo/bar.txt/").extension());
    ASSERT_EQ("", path("C:\\").extension());
    ASSERT_EQ(".", path("/foo/bar.").extension());
    ASSERT_EQ("", path("/foo/bar").extension());
    ASSERT_EQ(".cc", path("/foo/bar.txt/bar.cc").extension());
    ASSERT_EQ(".cc", path("/foo/bar.txt/bar.aa.bb.cc").extension());
    ASSERT_EQ(".", path("/foo/bar.txt/bar.").extension());
    ASSERT_EQ("", path("/foo/bar.txt/bar").extension());
    ASSERT_EQ("", path("/foo/.").extension());
    ASSERT_EQ("", path("/foo/..").extension());
    ASSERT_EQ("", path("/foo/.hidden").extension());
    ASSERT_EQ(".bar", path("/foo/..bar").extension());
}

TEST(StdFilesystem, stem)
{
    ASSERT_EQ("bar", path("/foo/bar.txt").stem());
    ASSERT_EQ("bar", path("c:\\foo\\bar.txt").stem());
    ASSERT_EQ(".bar", path("/foo/.bar").stem());
    ASSERT_EQ("", path("/").stem());
    ASSERT_EQ("", path("C:\\foo\\").stem());
    ASSERT_EQ(".", path("/foo/.").stem());
    ASSERT_EQ("", path("/foo/..").stem());
}

} // namespace nx::utils::filesystem
