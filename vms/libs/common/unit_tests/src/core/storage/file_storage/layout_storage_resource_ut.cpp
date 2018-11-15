#include <gtest/gtest.h>

#include <common/common_module.h>
#include <nx/core/access/access_types.h>

#include <nx/utils/test_support/test_with_temporary_directory.h>
#include <core/storage/file_storage/layout_storage_resource.h>

const QString kfileNameBase = "Test.nov";

const QString kPassword = "asdf";

constexpr qint64 kBufferSize = 1 << 18; // ~260k.

constexpr int kStreamCount = 3;
int streamSizes[kStreamCount] = { 15820, 216710, 258868 }; // Arbitrary numbers < kBufferSize.
const QString streamNames[kStreamCount] = { "stream1.txt", "stream2.avi", "stream3.avi" };

TEST(QnLayoutFileStorageResource, Basic)
{
    char buffer[kBufferSize];
    for (int i = 0; i < kBufferSize; i++)
        (unsigned char&) buffer[i] = (i + 5) % 255;

    QSharedPointer<QnCommonModule> module(new QnCommonModule(false, nx::core::access::Mode::direct));

    nx::utils::test::TestWithTemporaryDirectory TestDir("layout_storage_resource", QString());

    auto const fileName = QDir(TestDir.testDataDir()).filePath(kfileNameBase);
    QFile(fileName).remove();

    { // Write nov.
        QnLayoutFileStorageResourcePtr storage(new QnLayoutFileStorageResource(module.data()));
        storage->setUrl(fileName);
        storage->setPasswordToWrite(kPassword);

        for (int i = 0; i < kStreamCount; i++)
        {
            auto stream = storage->open(streamNames[i], QIODevice::WriteOnly);
            stream->write(buffer, streamSizes[i]);
            delete stream;
        }
    }

    {   // Read nov.
        QnLayoutFileStorageResourcePtr storage(new QnLayoutFileStorageResource(module.data()));
        storage->setUrl(fileName);
        storage->usePasswordToRead(kPassword);

        char readBuffer[kBufferSize];
        std::fill(readBuffer, readBuffer + kBufferSize, -1);

        for (int i = kStreamCount - 1; i >= 0; i--) // Reverse order (for fun).
        {
            auto stream = storage->open(streamNames[i], QIODevice::ReadOnly);
            stream->read(readBuffer, streamSizes[i]);
            for (int j = 0; j < streamSizes[i]; j++)
                ASSERT_EQ(buffer[j], readBuffer[j]);
            delete stream;
            std::fill(readBuffer, readBuffer + kBufferSize, -1);
        }
    }
}

