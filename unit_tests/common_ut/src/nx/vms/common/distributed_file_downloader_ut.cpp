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
static const qint64 kTestFileSize = 1024 * 1024 * 4 + 42;

} // namespace

class DistributedFileDownloaderTest: public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
        workingDirectory = QDir::temp().absoluteFilePath("__tmp_dfd_test");
        workingDirectory.removeRecursively();
        ASSERT_TRUE(QDir().mkdir(workingDirectory.absolutePath()));

        testFileName = kTestFileName;
        testFilePath = workingDirectory.absoluteFilePath(testFileName);

        downloader.reset(new DistributedFileDownloader(workingDirectory));
    }

    virtual void TearDown() override
    {
        downloader.reset();
        ASSERT_TRUE(workingDirectory.removeRecursively());
    }

    bool createTestFile(const QString& fileName, qint64 size)
    {
        QFile file(workingDirectory.absoluteFilePath(fileName));

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

    bool createDefaultTestFile()
    {
        if (!createTestFile(testFileName, kTestFileSize))
            return false;

        testFileMd5 = DistributedFileDownloader::calculateMd5(testFilePath);
        if (testFileMd5.isEmpty())
            return false;

        return true;
    }

    QDir workingDirectory;
    QString testFileName;
    QString testFilePath;
    QByteArray testFileMd5;
    QScopedPointer<DistributedFileDownloader> downloader;
};

TEST_F(DistributedFileDownloaderTest, inexistentFileRequests)
{
    const auto& fileInfo = downloader->fileInformation(testFileName);
    ASSERT_EQ(fileInfo.status, DownloaderFileInformation::Status::notFound);

    QByteArray buffer;

    ASSERT_EQ(downloader->readFileChunk(testFileName, 0, buffer),
        DistributedFileDownloader::ErrorCode::fileDoesNotExist);

    ASSERT_EQ(downloader->writeFileChunk(testFileName, 0, buffer),
        DistributedFileDownloader::ErrorCode::fileDoesNotExist);

    ASSERT_EQ(downloader->deleteFile(testFileName),
        DistributedFileDownloader::ErrorCode::fileDoesNotExist);
}

TEST_F(DistributedFileDownloaderTest, addNewFileWithoutInformation)
{
    ASSERT_EQ(downloader->addFile(testFileName),
        DistributedFileDownloader::ErrorCode::noError);

    const auto& fileInfo = downloader->fileInformation(testFileName);

    ASSERT_EQ(fileInfo.status, DownloaderFileInformation::Status::downloading);
    ASSERT_EQ(fileInfo.size, -1);
    ASSERT_TRUE(fileInfo.md5.isEmpty());
}

TEST_F(DistributedFileDownloaderTest, addNewFile)
{
    const QByteArray md5("md5_test");

    {
        DownloaderFileInformation fileInfo(testFileName);
        fileInfo.size = kTestFileSize;
        fileInfo.md5 = md5;

        ASSERT_EQ(downloader->addFile(fileInfo), DistributedFileDownloader::ErrorCode::noError);
    }

    const auto& fileInfo = downloader->fileInformation(testFileName);

    ASSERT_EQ(fileInfo.status, DownloaderFileInformation::Status::downloading);
    ASSERT_EQ(fileInfo.size, kTestFileSize);
    ASSERT_TRUE(fileInfo.md5 == md5);
    ASSERT_FALSE(fileInfo.downloadedChunks.isEmpty());
}

TEST_F(DistributedFileDownloaderTest, addDownloadedFile)
{
    ASSERT_TRUE(createDefaultTestFile());

    {
        DownloaderFileInformation fileInfo(testFileName);
        fileInfo.status = DownloaderFileInformation::Status::downloaded;

        ASSERT_EQ(downloader->addFile(fileInfo), DistributedFileDownloader::ErrorCode::noError);
    }

    const auto& fileInfo = downloader->fileInformation(testFileName);
    ASSERT_EQ(fileInfo.status, DownloaderFileInformation::Status::downloaded);
    ASSERT_EQ(fileInfo.size, kTestFileSize);
    ASSERT_TRUE(fileInfo.md5 == testFileMd5);
    ASSERT_TRUE(fileInfo.chunkSize > 0);
    ASSERT_EQ(fileInfo.downloadedChunks.size(),
        DistributedFileDownloader::calculateChunkCount(fileInfo.size, fileInfo.chunkSize));
}

TEST_F(DistributedFileDownloaderTest, addDownloadedFileAsNotDownloaded)
{
    ASSERT_TRUE(createDefaultTestFile());

    {
        DownloaderFileInformation fileInfo(testFileName);
        fileInfo.md5 = testFileMd5;

        ASSERT_EQ(downloader->addFile(fileInfo), DistributedFileDownloader::ErrorCode::noError);
    }

    // Valid file is already exists, so file should became downloaded.
    const auto& fileInfo = downloader->fileInformation(testFileName);
    ASSERT_EQ(fileInfo.status, DownloaderFileInformation::Status::downloaded);
    ASSERT_EQ(fileInfo.size, kTestFileSize);
    ASSERT_TRUE(fileInfo.md5 == testFileMd5);
    ASSERT_TRUE(fileInfo.chunkSize > 0);
    ASSERT_EQ(fileInfo.downloadedChunks.size(),
        DistributedFileDownloader::calculateChunkCount(fileInfo.size, fileInfo.chunkSize));
}

TEST_F(DistributedFileDownloaderTest, addFileWithWrongChecksum)
{
    ASSERT_TRUE(createDefaultTestFile());

    {
        DownloaderFileInformation fileInfo(testFileName);
        fileInfo.status = DownloaderFileInformation::Status::downloaded;
        fileInfo.md5 = "invalid_md5";

        ASSERT_EQ(downloader->addFile(fileInfo),
            DistributedFileDownloader::ErrorCode::invalidChecksum);
    }

    const auto& fileInfo = downloader->fileInformation(testFileName);
    ASSERT_EQ(fileInfo.status, DownloaderFileInformation::Status::notFound);
}

TEST_F(DistributedFileDownloaderTest, fileDuplicate)
{
    ASSERT_EQ(downloader->addFile(testFileName),
        DistributedFileDownloader::ErrorCode::noError);

    ASSERT_EQ(downloader->addFile(testFileName),
        DistributedFileDownloader::ErrorCode::fileAlreadyExists);
}

TEST_F(DistributedFileDownloaderTest, fileDeletion)
{
    ASSERT_TRUE(createTestFile(testFileName, 0));

    ASSERT_EQ(downloader->deleteFile(testFileName),
        DistributedFileDownloader::ErrorCode::fileDoesNotExist);

    // Delete download storing the data.
    ASSERT_EQ(downloader->addFile(testFileName),
        DistributedFileDownloader::ErrorCode::noError);
    ASSERT_EQ(downloader->deleteFile(testFileName, false),
        DistributedFileDownloader::ErrorCode::noError);
    ASSERT_TRUE(QFile::exists(testFilePath));

    // Delete download with its data.
    ASSERT_EQ(downloader->addFile(testFileName),
        DistributedFileDownloader::ErrorCode::noError);
    ASSERT_EQ(downloader->deleteFile(testFileName),
        DistributedFileDownloader::ErrorCode::noError);
    ASSERT_FALSE(QFile::exists(testFilePath));
}

TEST_F(DistributedFileDownloaderTest, findDownloadsForDownloadedFiles)
{
    ASSERT_TRUE(createDefaultTestFile());

    {
        DownloaderFileInformation fileInfo(testFileName);
        fileInfo.status = DownloaderFileInformation::Status::downloaded;

        ASSERT_EQ(this->downloader->addFile(fileInfo),
            DistributedFileDownloader::ErrorCode::noError);
    }

    DistributedFileDownloader downloader(workingDirectory);

    const auto& fileInfo = downloader.fileInformation(testFileName);
    ASSERT_EQ(fileInfo.status, DownloaderFileInformation::Status::downloaded);
    ASSERT_EQ(fileInfo.size, kTestFileSize);
    ASSERT_TRUE(fileInfo.md5 == testFileMd5);
    ASSERT_TRUE(fileInfo.md5 == DistributedFileDownloader::calculateMd5(testFilePath));
    ASSERT_TRUE(fileInfo.chunkSize > 0);
    ASSERT_EQ(fileInfo.downloadedChunks.size(),
        DistributedFileDownloader::calculateChunkCount(fileInfo.size, fileInfo.chunkSize));
}

TEST_F(DistributedFileDownloaderTest, findDownloadsForNewFiles)
{
    ASSERT_EQ(this->downloader->addFile(testFileName),
        DistributedFileDownloader::ErrorCode::noError);

    DistributedFileDownloader downloader(workingDirectory);

    const auto& fileInfo = downloader.fileInformation(testFileName);
    ASSERT_EQ(fileInfo.status, DownloaderFileInformation::Status::downloading);
}

TEST_F(DistributedFileDownloaderTest, findDownloadsForFilesInProgress)
{
    ASSERT_TRUE(createDefaultTestFile());

    const auto targetFileName = testFileName + ".new";
    const auto targetFilePath = workingDirectory.absoluteFilePath(targetFileName);
    QByteArray md5BeforeFind;

    {
        DownloaderFileInformation fileInfo(testFileName);
        fileInfo.status = DownloaderFileInformation::Status::downloaded;

        ASSERT_EQ(this->downloader->addFile(fileInfo),
            DistributedFileDownloader::ErrorCode::noError);

        fileInfo = DownloaderFileInformation(targetFileName);
        fileInfo.md5 = testFileMd5;
        fileInfo.size = kTestFileSize;

        ASSERT_EQ(this->downloader->addFile(fileInfo),
            DistributedFileDownloader::ErrorCode::noError);

        QByteArray buffer;

        ASSERT_EQ(this->downloader->readFileChunk(testFileName, 0, buffer),
            DistributedFileDownloader::ErrorCode::noError);
        ASSERT_EQ(this->downloader->writeFileChunk(targetFileName, 0, buffer),
            DistributedFileDownloader::ErrorCode::noError);

        md5BeforeFind = DistributedFileDownloader::calculateMd5(targetFilePath);
    }

    DistributedFileDownloader downloader(workingDirectory);

    const auto& fileInfo = downloader.fileInformation(targetFileName);
    ASSERT_EQ(fileInfo.status, DownloaderFileInformation::Status::downloading);
    ASSERT_TRUE(md5BeforeFind == DistributedFileDownloader::calculateMd5(targetFilePath));
}

TEST_F(DistributedFileDownloaderTest, findCorruptedFiles)
{
    {
        DownloaderFileInformation fileInfo(testFileName);
        fileInfo.size = 1;
        fileInfo.md5 = "invalid_md5";

        ASSERT_EQ(this->downloader->addFile(fileInfo),
            DistributedFileDownloader::ErrorCode::noError);

        const QByteArray buffer("_");
        ASSERT_EQ(this->downloader->writeFileChunk(testFileName, 0, buffer),
            DistributedFileDownloader::ErrorCode::noError);

        fileInfo = downloader->fileInformation(testFileName);
        ASSERT_EQ(fileInfo.status, DownloaderFileInformation::Status::corrupted);
    }

    DistributedFileDownloader downloader(workingDirectory);

    const auto& fileInfo = downloader.fileInformation(testFileName);
    ASSERT_EQ(fileInfo.status, DownloaderFileInformation::Status::corrupted);
}

TEST_F(DistributedFileDownloaderTest, findFileAfterDeletion)
{
    ASSERT_TRUE(createTestFile(testFileName, 0));

    ASSERT_EQ(this->downloader->addFile(testFileName),
        DistributedFileDownloader::ErrorCode::noError);
    ASSERT_EQ(this->downloader->deleteFile(testFileName),
        DistributedFileDownloader::ErrorCode::noError);

    DistributedFileDownloader downloader(workingDirectory);

    const auto& fileInfo = downloader.fileInformation(testFileName);
    ASSERT_EQ(fileInfo.status, DownloaderFileInformation::Status::notFound);
}

TEST_F(DistributedFileDownloaderTest, chunkOperations)
{
    ASSERT_TRUE(createDefaultTestFile());

    const auto targetFileName = testFileName + ".new";

    {
        DownloaderFileInformation fileInfo(testFileName);
        fileInfo.status = DownloaderFileInformation::Status::downloaded;

        ASSERT_EQ(downloader->addFile(fileInfo),
            DistributedFileDownloader::ErrorCode::noError);

        DownloaderFileInformation targetFileInfo(targetFileName);
        targetFileInfo.md5 = testFileMd5;
        targetFileInfo.size = kTestFileSize;

        ASSERT_EQ(downloader->addFile(targetFileInfo),
            DistributedFileDownloader::ErrorCode::noError);
    }

    const auto& fileInfo = downloader->fileInformation(testFileName);

    for (int i = 0; i < fileInfo.downloadedChunks.size(); ++i)
    {
        QByteArray buffer;

        ASSERT_EQ(downloader->readFileChunk(testFileName, i, buffer),
            DistributedFileDownloader::ErrorCode::noError);
        ASSERT_EQ(downloader->writeFileChunk(targetFileName, i, buffer),
            DistributedFileDownloader::ErrorCode::noError);
    }

    const auto& targetFileInfo = downloader->fileInformation(targetFileName);
    ASSERT_EQ(targetFileInfo.status, DownloaderFileInformation::Status::downloaded);

    const auto targetMd5 = DistributedFileDownloader::calculateMd5(
        workingDirectory.absoluteFilePath(targetFileInfo.name));
    ASSERT_TRUE(testFileMd5 == targetMd5);
}

TEST_F(DistributedFileDownloaderTest, readByInvalidChunkIndex)
{
    ASSERT_TRUE(createDefaultTestFile());

    DownloaderFileInformation fileInformation(testFileName);
    fileInformation.status = DownloaderFileInformation::Status::downloaded;

    ASSERT_EQ(downloader->addFile(fileInformation),
        DistributedFileDownloader::ErrorCode::noError);

    QByteArray buffer;
    ASSERT_EQ(downloader->readFileChunk(testFileName, 42, buffer),
        DistributedFileDownloader::ErrorCode::invalidChunkIndex);
}

TEST_F(DistributedFileDownloaderTest, writeInvalidChunks)
{
    ASSERT_TRUE(createDefaultTestFile());

    DownloaderFileInformation fileInformation(testFileName);
    fileInformation.size = 1;

    ASSERT_EQ(downloader->addFile(fileInformation),
        DistributedFileDownloader::ErrorCode::noError);

    QByteArray buffer;

    ASSERT_EQ(downloader->writeFileChunk(testFileName, 42, buffer),
        DistributedFileDownloader::ErrorCode::invalidChunkIndex);

    buffer = "some_bytes";
    ASSERT_EQ(downloader->writeFileChunk(testFileName, 0, buffer),
        DistributedFileDownloader::ErrorCode::invalidChunkSize);
}

TEST_F(DistributedFileDownloaderTest, writeToDownloadedFile)
{
    ASSERT_TRUE(createDefaultTestFile());

    DownloaderFileInformation fileInformation(testFileName);
    fileInformation.status = DownloaderFileInformation::Status::downloaded;

    ASSERT_EQ(downloader->addFile(fileInformation),
        DistributedFileDownloader::ErrorCode::noError);

    QByteArray buffer;
    ASSERT_EQ(downloader->writeFileChunk(testFileName, 0, buffer),
        DistributedFileDownloader::ErrorCode::ioError);
}

TEST_F(DistributedFileDownloaderTest, fileCorruptionDuringDownload)
{
    {
        DownloaderFileInformation fileInfo(testFileName);
        fileInfo.md5 = "md5_corrupted";
        fileInfo.size = 1;

        ASSERT_EQ(downloader->addFile(fileInfo), DistributedFileDownloader::ErrorCode::noError);
    }

    ASSERT_EQ(downloader->writeFileChunk(testFileName, 0, "1"),
        DistributedFileDownloader::ErrorCode::noError);

    const auto& fileInformation = downloader->fileInformation(testFileName);
    ASSERT_EQ(fileInformation.status, DownloaderFileInformation::Status::corrupted);
}

TEST_F(DistributedFileDownloaderTest, getChecksums)
{
    ASSERT_TRUE(createDefaultTestFile());

    DownloaderFileInformation fileInformation(testFileName);
    fileInformation.status = DownloaderFileInformation::Status::downloaded;

    ASSERT_EQ(downloader->addFile(fileInformation),
        DistributedFileDownloader::ErrorCode::noError);

    const auto& fileInfo = downloader->fileInformation(testFileName);

    const auto checksums = downloader->getChunkChecksums(testFileName);
    const auto chunkCount = DistributedFileDownloader::calculateChunkCount(
        fileInfo.size, fileInfo.chunkSize);
    ASSERT_EQ(checksums.size(), chunkCount);
}

TEST_F(DistributedFileDownloaderTest, getChecksumsAfterDownload)
{
    ASSERT_TRUE(createTestFile(testFileName, 1024));

    DownloaderFileInformation fileInfo(testFileName);
    fileInfo.status = DownloaderFileInformation::Status::downloaded;

    ASSERT_EQ(downloader->addFile(fileInfo),
        DistributedFileDownloader::ErrorCode::noError);

    fileInfo = downloader->fileInformation(testFileName);

    QByteArray data;
    ASSERT_EQ(downloader->readFileChunk(testFileName, 0, data),
        DistributedFileDownloader::ErrorCode::noError);

    DownloaderFileInformation targetFileInfo(testFileName + ".new");
    targetFileInfo.size = fileInfo.size;
    targetFileInfo.md5 = fileInfo.md5;
    ASSERT_EQ(downloader->addFile(targetFileInfo),
        DistributedFileDownloader::ErrorCode::noError);

    ASSERT_EQ(downloader->writeFileChunk(targetFileInfo.name, 0, data),
        DistributedFileDownloader::ErrorCode::noError);

    const auto checksums = downloader->getChunkChecksums(targetFileInfo.name);
    ASSERT_EQ(checksums.size(), 1);
    ASSERT_FALSE(checksums[0].isEmpty());
}

} // namespace test
} // namespace common
} // namespace vms
} // namespace nx
