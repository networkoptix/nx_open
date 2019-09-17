#define GTEST_HAS_TR1_TUPLE     0
#define GTEST_USE_OWN_TR1_TUPLE 1

#define GTEST_HAS_POSIX_RE 0

#include <gtest/gtest.h>
#include <plugins/storage/file_storage/file_storage_resource.h>
#include "media_server_module_fixture.h"
#include <test_support/utils.h>

namespace nx::vms::server::test
{

class StorageTest:
    public MediaServerModuleFixture
{
public:
    StorageTest()
    {
    }

    virtual void SetUp() override
    {
        MediaServerModuleFixture::SetUp();

        workDirResource = std::make_unique<nx::ut::utils::WorkDirResource>();
        ASSERT_TRUE((bool)workDirResource->getDirName());

        storage.reset(new QnFileStorageResource(&serverModule()));
        const QString workDirPath = *workDirResource->getDirName();
        storage->setUrl(workDirPath);
        auto result = storage->initOrUpdate() == Qn::StorageInit_Ok;
        ASSERT_TRUE(result);
    }

    std::unique_ptr<nx::ut::utils::WorkDirResource> workDirResource;
    StorageResourcePtr storage;
};

TEST_F(StorageTest, StorageMetrics)
{
    const int kIterations = 3;
    const QByteArray kPattern = "12345";

    for (int i = 0; i < kIterations; ++i)
    {
        auto file = std::unique_ptr<QIODevice>(
            storage->open(lm("file%1").arg(i), QIODevice::WriteOnly | QIODevice::Truncate));
        file->write(kPattern);

        file = std::unique_ptr<QIODevice>(
            storage->open(lm("file%1").arg(i), QIODevice::ReadOnly));
        file->read(kPattern.length() * 2); //< Try to read more than file length.
    }

    auto bytesWritten = storage->getAndResetMetric(&StorageResource::Metrics::bytesWritten);
    auto bytesRead = storage->getAndResetMetric(&StorageResource::Metrics::bytesRead);
    ASSERT_EQ(kPattern.length() * kIterations, bytesWritten);
    ASSERT_EQ(kPattern.length() * kIterations, bytesRead);

    bytesWritten = storage->getAndResetMetric(&StorageResource::Metrics::bytesWritten);
    bytesRead = storage->getAndResetMetric(&StorageResource::Metrics::bytesWritten);
    ASSERT_EQ(0, bytesWritten);
    ASSERT_EQ(0, bytesRead);
}

} // nx::vms::server::test
