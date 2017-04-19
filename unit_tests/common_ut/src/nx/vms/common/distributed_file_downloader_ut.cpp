#include <gtest/gtest.h>

#include <QtCore/QDir>
#include <QtCore/QFile>

#include <nx/vms/common/distributed_file_downloader.h>

namespace nx {
namespace vms {
namespace common {
namespace test {

namespace {

static const QString kTestFileName("test.dat");
static const qint64 kTestFileSize = 1024 * 1024 * 4;
static const qint64 kSmallTestFileSize = 0;

} // namespace

class DistributedFileDownloaderTest: public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
        workingDirectory = QDir::temp().absoluteFilePath("__tmp_dfd_test");
        workingDirectory.removeRecursively();
        ASSERT_TRUE(QDir().mkdir(workingDirectory.absolutePath()));
    }

    virtual void TearDown() override
    {
        ASSERT_TRUE(workingDirectory.removeRecursively());
    }

    bool createTestFile(const QString& fileName, qint64 size)
    {
        QFile file(fileName);

        if (!file.open(QFile::WriteOnly))
            return false;

        for (qint64 i = 0; i < size; ++i)
        {
            const char c = static_cast<char>(rand());
            if (file.write(&c, 1) != 1)
                return false;
        }

        file.close();

        return true;
    }

    QDir workingDirectory;
};

TEST_F(DistributedFileDownloaderTest, inexistentFileRequests)
{
    const auto fileName = workingDirectory.absoluteFilePath(kTestFileName);

    DistributedFileDownloader downloader;

    const auto& fileInfo = downloader.fileInformation(fileName);
    ASSERT_EQ(fileInfo.status, DistributedFileDownloader::FileStatus::notFound);

    QByteArray buffer;

    ASSERT_EQ(downloader.readFileChunk(fileName, 0, buffer),
        DistributedFileDownloader::ErrorCode::fileDoesNotExist);

    ASSERT_EQ(downloader.writeFileChunk(fileName, 0, buffer),
        DistributedFileDownloader::ErrorCode::fileDoesNotExist);

    ASSERT_EQ(downloader.deleteFile(fileName),
        DistributedFileDownloader::ErrorCode::fileDoesNotExist);
}

TEST_F(DistributedFileDownloaderTest, newFileAddition)
{
    const auto fileName = workingDirectory.absoluteFilePath(kTestFileName);

    const QByteArray md5("md5_test");

    // Add file without information.
    {
        DistributedFileDownloader downloader;

        ASSERT_EQ(downloader.addFile(fileName),
            DistributedFileDownloader::ErrorCode::noError);

        const auto& fileInfo = downloader.fileInformation(fileName);

        ASSERT_EQ(fileInfo.status, DistributedFileDownloader::FileStatus::downloading);
        ASSERT_EQ(fileInfo.size, -1);
        ASSERT_TRUE(fileInfo.md5.isEmpty());
    }

    // Add file with md5 and size.
    {
        DistributedFileDownloader downloader;

        {
            DistributedFileDownloader::FileInformation fileInformation(fileName);
            fileInformation.size = kTestFileSize;
            fileInformation.md5 = md5;

            ASSERT_EQ(downloader.addFile(fileInformation),
                DistributedFileDownloader::ErrorCode::noError);
        }

        const auto& fileInformation = downloader.fileInformation(fileName);

        ASSERT_EQ(fileInformation.status, DistributedFileDownloader::FileStatus::downloading);
        ASSERT_EQ(fileInformation.size, kTestFileSize);
        ASSERT_TRUE(fileInformation.md5 == md5);
        ASSERT_FALSE(fileInformation.downloadedChunks.isEmpty());
    }
}

TEST_F(DistributedFileDownloaderTest, fileAddition)
{
    const auto fileName = workingDirectory.absoluteFilePath(kTestFileName);
    ASSERT_TRUE(createTestFile(fileName, kTestFileSize));

    const auto md5 = DistributedFileDownloader::calculateMd5(fileName);
    ASSERT_FALSE(md5.isEmpty());

    {
        DistributedFileDownloader downloader;

        DistributedFileDownloader::FileInformation fileInformation(fileName);
        fileInformation.status = DistributedFileDownloader::FileStatus::downloaded;

        ASSERT_EQ(downloader.addFile(fileInformation),
            DistributedFileDownloader::ErrorCode::noError);

        const auto& fileInfo = downloader.fileInformation(fileName);
        ASSERT_EQ(fileInfo.status, DistributedFileDownloader::FileStatus::downloaded);
        ASSERT_EQ(fileInfo.size, kTestFileSize);
        ASSERT_TRUE(fileInfo.md5 == md5);
        ASSERT_TRUE(fileInfo.chunkSize > 0);
        ASSERT_EQ(fileInfo.downloadedChunks.size(),
            DistributedFileDownloader::calculateChunkCount(fileInfo.size, fileInfo.chunkSize));
    }

    {
        DistributedFileDownloader downloader;

        DistributedFileDownloader::FileInformation fileInformation(fileName);
        // Add file with md5. Valid file is already exists, so file should became downloaded.
        fileInformation.md5 = md5;

        ASSERT_EQ(downloader.addFile(fileInformation),
            DistributedFileDownloader::ErrorCode::noError);

        const auto& fileInfo = downloader.fileInformation(fileName);
        ASSERT_EQ(fileInfo.status, DistributedFileDownloader::FileStatus::downloaded);
        ASSERT_EQ(fileInfo.size, kTestFileSize);
        ASSERT_TRUE(fileInfo.md5 == md5);
        ASSERT_TRUE(fileInfo.chunkSize > 0);
        ASSERT_EQ(fileInfo.downloadedChunks.size(),
            DistributedFileDownloader::calculateChunkCount(fileInfo.size, fileInfo.chunkSize));
    }

    {
        DistributedFileDownloader downloader;

        DistributedFileDownloader::FileInformation fileInformation(fileName);
        fileInformation.status = DistributedFileDownloader::FileStatus::downloaded;
        fileInformation.md5 = md5;
        // Add file using wrong checksum.
        fileInformation.md5[0] = fileInformation.md5[0] + 1;

        ASSERT_EQ(downloader.addFile(fileInformation),
            DistributedFileDownloader::ErrorCode::invalidChecksum);

        const auto& fileInfo = downloader.fileInformation(fileName);
        ASSERT_EQ(fileInfo.status, DistributedFileDownloader::FileStatus::notFound);
    }
}

TEST_F(DistributedFileDownloaderTest, fileDuplicate)
{
    const auto fileName = workingDirectory.absoluteFilePath(kTestFileName);

    DistributedFileDownloader downloader;

    ASSERT_EQ(downloader.addFile(fileName),
        DistributedFileDownloader::ErrorCode::noError);

    ASSERT_EQ(downloader.addFile(fileName),
        DistributedFileDownloader::ErrorCode::fileAlreadyExists);
}

TEST_F(DistributedFileDownloaderTest, fileDeletion)
{
    const auto fileName = workingDirectory.absoluteFilePath(kTestFileName);
    ASSERT_TRUE(createTestFile(fileName, kSmallTestFileSize));

    DistributedFileDownloader downloader;

    ASSERT_EQ(downloader.deleteFile(fileName),
        DistributedFileDownloader::ErrorCode::fileDoesNotExist);

    // Delete download storing the data.
    ASSERT_EQ(downloader.addFile(fileName),
        DistributedFileDownloader::ErrorCode::noError);
    ASSERT_EQ(downloader.deleteFile(fileName, false),
        DistributedFileDownloader::ErrorCode::noError);
    ASSERT_TRUE(QFile::exists(fileName));

    // Delete download with its data.
    ASSERT_EQ(downloader.addFile(fileName),
        DistributedFileDownloader::ErrorCode::noError);
    ASSERT_EQ(downloader.deleteFile(fileName),
        DistributedFileDownloader::ErrorCode::noError);
    ASSERT_FALSE(QFile::exists(fileName));
}

TEST_F(DistributedFileDownloaderTest, findDownloads)
{
    const auto fileName = workingDirectory.absoluteFilePath(kTestFileName);
    ASSERT_TRUE(createTestFile(fileName, kTestFileSize));

    const auto md5 = DistributedFileDownloader::calculateMd5(fileName);
    ASSERT_FALSE(md5.isEmpty());

    // Check if findDownloads works OK for downloaded files.
    {
        DistributedFileDownloader downloader;

        DistributedFileDownloader::FileInformation fileInformation(fileName);
        fileInformation.status = DistributedFileDownloader::FileStatus::downloaded;

        ASSERT_EQ(downloader.addFile(fileInformation),
            DistributedFileDownloader::ErrorCode::noError);
    }

    {
        DistributedFileDownloader downloader;

        ASSERT_EQ(downloader.findDownloads(workingDirectory.absolutePath()),
            DistributedFileDownloader::ErrorCode::noError);

        const auto& fileInfo = downloader.fileInformation(fileName);
        ASSERT_EQ(fileInfo.status, DistributedFileDownloader::FileStatus::downloaded);
        ASSERT_EQ(fileInfo.size, kTestFileSize);
        ASSERT_TRUE(fileInfo.md5 == md5);
        ASSERT_TRUE(fileInfo.chunkSize > 0);
        ASSERT_EQ(fileInfo.downloadedChunks.size(),
            DistributedFileDownloader::calculateChunkCount(fileInfo.size, fileInfo.chunkSize));

        ASSERT_EQ(downloader.deleteFile(fileName),
            DistributedFileDownloader::ErrorCode::noError);
    }

    // Check if findDownloads works OK for new downloads.
    {
        DistributedFileDownloader downloader;

        // Now we don't have a file, just creating download.
        ASSERT_EQ(downloader.addFile(fileName),
            DistributedFileDownloader::ErrorCode::noError);
    }

    {
        DistributedFileDownloader downloader;

        ASSERT_EQ(downloader.findDownloads(workingDirectory.absolutePath()),
            DistributedFileDownloader::ErrorCode::noError);

        const auto fileInfo = downloader.fileInformation(fileName);
        ASSERT_EQ(fileInfo.status, DistributedFileDownloader::FileStatus::downloading);
    }
}

TEST_F(DistributedFileDownloaderTest, findFileAfterDeletion)
{
    const auto fileName = workingDirectory.absoluteFilePath(kTestFileName);
    ASSERT_TRUE(createTestFile(fileName, kSmallTestFileSize));

    DistributedFileDownloader downloader;

    ASSERT_EQ(downloader.addFile(fileName),
        DistributedFileDownloader::ErrorCode::noError);
    ASSERT_EQ(downloader.deleteFile(fileName),
        DistributedFileDownloader::ErrorCode::noError);

    ASSERT_EQ(downloader.findDownloads(workingDirectory.absolutePath()),
        DistributedFileDownloader::ErrorCode::noError);

    const auto& fileInfo = downloader.fileInformation(fileName);
    ASSERT_EQ(fileInfo.status, DistributedFileDownloader::FileStatus::notFound);
}

TEST_F(DistributedFileDownloaderTest, chunkOperations)
{
    const auto fileName = workingDirectory.absoluteFilePath(kTestFileName);
    ASSERT_TRUE(createTestFile(fileName, kTestFileSize));

    const auto md5 = DistributedFileDownloader::calculateMd5(fileName);
    ASSERT_FALSE(md5.isEmpty());

    const auto targetFileName = workingDirectory.absoluteFilePath(kTestFileName + ".new");

    DistributedFileDownloader downloader;

    {
        DistributedFileDownloader::FileInformation fileInformation(fileName);
        fileInformation.status = DistributedFileDownloader::FileStatus::downloaded;

        ASSERT_EQ(downloader.addFile(fileInformation),
            DistributedFileDownloader::ErrorCode::noError);

        DistributedFileDownloader::FileInformation targetFileInformation(targetFileName);
        targetFileInformation.md5 = md5;
        targetFileInformation.size = kTestFileSize;

        ASSERT_EQ(downloader.addFile(targetFileInformation),
            DistributedFileDownloader::ErrorCode::noError);
    }

    const auto& fileInformation = downloader.fileInformation(fileName);

    for (int i = 0; i < fileInformation.downloadedChunks.size(); ++i)
    {
        QByteArray buffer;

        ASSERT_EQ(downloader.readFileChunk(fileName, i, buffer),
            DistributedFileDownloader::ErrorCode::noError);
        ASSERT_EQ(downloader.writeFileChunk(targetFileName, i, buffer),
            DistributedFileDownloader::ErrorCode::noError);
    }

    const auto& targetFileInformation = downloader.fileInformation(targetFileName);
    ASSERT_EQ(targetFileInformation.status, DistributedFileDownloader::FileStatus::downloaded);

    const auto targetMd5 = DistributedFileDownloader::calculateMd5(targetFileInformation.name);
    ASSERT_TRUE(md5 == targetMd5);
}

TEST_F(DistributedFileDownloaderTest, invalidChunkOperations)
{
    const auto fileName = workingDirectory.absoluteFilePath(kTestFileName);
    ASSERT_TRUE(createTestFile(fileName, kTestFileSize));

    const auto md5 = DistributedFileDownloader::calculateMd5(fileName);
    ASSERT_FALSE(md5.isEmpty());

    QByteArray buffer;

    {
        DistributedFileDownloader downloader;

        DistributedFileDownloader::FileInformation fileInformation(fileName);
        fileInformation.status = DistributedFileDownloader::FileStatus::downloaded;

        ASSERT_EQ(downloader.addFile(fileInformation),
            DistributedFileDownloader::ErrorCode::noError);

        // Try to read by an invalid chunk index.
        ASSERT_EQ(downloader.readFileChunk(fileName, 42, buffer),
            DistributedFileDownloader::ErrorCode::invalidChunkIndex);

        // Try to write into a downloaded file.
        ASSERT_EQ(downloader.writeFileChunk(fileName, 0, buffer),
            DistributedFileDownloader::ErrorCode::ioError);
    }

    {
        DistributedFileDownloader downloader;

        DistributedFileDownloader::FileInformation fileInformation(fileName);
        fileInformation.size = DistributedFileDownloader::calculateFileSize(fileName);

        ASSERT_EQ(downloader.addFile(fileInformation),
            DistributedFileDownloader::ErrorCode::noError);

        // Try to write by an invalid chunk index.
        ASSERT_EQ(downloader.writeFileChunk(fileName, 42, buffer),
            DistributedFileDownloader::ErrorCode::invalidChunkIndex);
    }
}

TEST_F(DistributedFileDownloaderTest, fileCorruption)
{
    constexpr int kFileSize = 1;

    const auto fileName = workingDirectory.absoluteFilePath(kTestFileName);

    DistributedFileDownloader downloader;

    {
        DistributedFileDownloader::FileInformation fileInformation(fileName);
        fileInformation.md5 = "md5_corrupted";
        fileInformation.size = kFileSize;

        ASSERT_EQ(downloader.addFile(fileInformation),
            DistributedFileDownloader::ErrorCode::noError);
    }

    ASSERT_EQ(downloader.writeFileChunk(fileName, 0, "1"),
        DistributedFileDownloader::ErrorCode::noError);

    const auto& fileInformation = downloader.fileInformation(fileName);
    ASSERT_EQ(fileInformation.status, DistributedFileDownloader::FileStatus::corrupted);
}

} // namespace test
} // namespace common
} // namespace vms
} // namespace nx
