#include <memory>
#include <algorithm>
#include <utility>
#include <cstring>
#include <string>
#include <random>
#include <unordered_map>

#include <nx/utils/thread/long_runnable.h>
#include <nx/utils/random_qt_device.h>
#include <utils/common/writer_pool.h>
#include <core/resource/storage_plugin_factory.h>
#include <core/resource_management/resource_pool.h>
#include <recorder/storage_manager.h>
#include <plugins/plugin_manager.h>
#include <plugins/storage/file_storage/file_storage_resource.h>
#include <plugins/storage/third_party_storage_resource/third_party_storage_resource.h>
#include <media_server/settings.h>
#include <utils/common/util.h>
#include <media_server/media_server_module.h>

#if !defined(_WIN32)
    #include <platform/platform_abstraction.h>
#endif

#define GTEST_HAS_TR1_TUPLE 0
#define GTEST_USE_OWN_TR1_TUPLE 1

#include <sstream>
#define GTEST_HAS_POSIX_RE 0
#include <gtest/gtest.h>

#include <test_support/utils.h>

#include "media_server_module_fixture.h"
#include <test_support/test_file_storage.h>

struct ExpectedFileInfo
{
    bool isDir;
    QString baseName;
    QString ext;
    QString dirPath;
    QString fileName;
    QString absolutePath;
};

class FileInfoTest: public ::testing::TestWithParam<ExpectedFileInfo> { };

TEST_P(FileInfoTest, Constructor)
{
    const auto expected = GetParam();
    QnAbstractStorageResource::FileInfo fileInfo(expected.absolutePath, 42, expected.isDir);
    ASSERT_EQ(expected.isDir, fileInfo.isDir());
    ASSERT_EQ(expected.baseName, fileInfo.baseName());
    ASSERT_EQ(expected.ext, fileInfo.extension());
    ASSERT_EQ(expected.dirPath, fileInfo.absoluteDirPath());
    ASSERT_EQ(expected.fileName, fileInfo.fileName());
    ASSERT_EQ(expected.absolutePath, fileInfo.absoluteFilePath());
}

INSTANTIATE_TEST_CASE_P(
    FileInfoConstructorTest,
    FileInfoTest,
    ::testing::Values<ExpectedFileInfo>(
        ExpectedFileInfo{false, "file", "ext", "/some/path", "file.ext", "/some/path/file.ext"},
        ExpectedFileInfo{false, "file", "ext", "smb://user:password@host:port/some/path", "file.ext",
            "smb://user:password@host:port/some/path/file.ext"},
        ExpectedFileInfo{true, "dir", "", "/some/path/dir", "dir", "/some/path/dir"},
        ExpectedFileInfo{true, "dir", "", "smb://user:password@host:port/some/path/dir", "dir",
            "smb://user:password@host:port/some/path/dir"}));
