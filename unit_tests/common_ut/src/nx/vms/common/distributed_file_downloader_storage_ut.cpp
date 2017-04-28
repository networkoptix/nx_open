#include <gtest/gtest.h>

#include <QtCore/QDir>
#include <QtCore/QFile>

#include <nx/vms/common/private/distributed_file_downloader/storage.h>

namespace nx {
namespace vms {
namespace common {
namespace test {

namespace {

static const QString kTestFileName("test.dat");
static const qint64 kTestFileSize = 1024 * 1024 * 4 + 42;

} // namespace

using distributed_file_downloader::Storage;

class DistributedFileDownloaderStorageTest: public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
        workingDirectory = QDir::temp().absoluteFilePath("__tmp_dfd_test");
        workingDirectory.removeRecursively();
        ASSERT_TRUE(QDir().mkdir(workingDirectory.absolutePath()));

        testFileName = kTestFileName;
        testFilePath = workingDirectory.absoluteFilePath(testFileName);

        downloaderStorage.reset(new Storage(workingDirectory));
    }

    virtual void TearDown() override
    {
        downloaderStorage.reset();
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

        testFileMd5 = Storage::calculateMd5(testFilePath);
        if (testFileMd5.isEmpty())
            return false;

        return true;
    }

    bool createDefaultTestFile()
    {
        return createTestFile(testFileName, kTestFileSize);
    }

    QDir workingDirectory;
    QString testFileName;
    QString testFilePath;
    QByteArray testFileMd5;
    QScopedPointer<Storage> downloaderStorage;
};

TEST_F(DistributedFileDownloaderStorageTest, inexistentFileRequests)
{
    const auto& fileInfo = downloaderStorage->fileInformation(testFileName);
    ASSERT_EQ(fileInfo.status, DownloaderFileInformation::Status::notFound);

    QByteArray buffer;

    ASSERT_EQ(downloaderStorage->readFileChunk(testFileName, 0, buffer),
        DistributedFileDownloader::ErrorCode::fileDoesNotExist);

    ASSERT_EQ(downloaderStorage->writeFileChunk(testFileName, 0, buffer),
        DistributedFileDownloader::ErrorCode::fileDoesNotExist);

    ASSERT_EQ(downloaderStorage->deleteFile(testFileName),
        DistributedFileDownloader::ErrorCode::fileDoesNotExist);
}

TEST_F(DistributedFileDownloaderStorageTest, addNewFileWithoutInformation)
{
    ASSERT_EQ(downloaderStorage->addFile(testFileName),
        DistributedFileDownloader::ErrorCode::noError);

    const auto& fileInfo = downloaderStorage->fileInformation(testFileName);

    ASSERT_EQ(fileInfo.status, DownloaderFileInformation::Status::downloading);
    ASSERT_EQ(fileInfo.size, -1);
    ASSERT_TRUE(fileInfo.md5.isEmpty());
}

TEST_F(DistributedFileDownloaderStorageTest, addNewFile)
{
    const QByteArray md5("md5_test");

    {
        DownloaderFileInformation fileInfo(testFileName);
        fileInfo.size = kTestFileSize;
        fileInfo.md5 = md5;

        ASSERT_EQ(downloaderStorage->addFile(fileInfo),
            DistributedFileDownloader::ErrorCode::noError);
    }

    const auto& fileInfo = downloaderStorage->fileInformation(testFileName);

    ASSERT_EQ(fileInfo.status, DownloaderFileInformation::Status::downloading);
    ASSERT_EQ(fileInfo.size, kTestFileSize);
    ASSERT_TRUE(fileInfo.md5 == md5);
    ASSERT_FALSE(fileInfo.downloadedChunks.isEmpty());
}

TEST_F(DistributedFileDownloaderStorageTest, addDownloadedFile)
{
    ASSERT_TRUE(createDefaultTestFile());

    {
        DownloaderFileInformation fileInfo(testFileName);
        fileInfo.status = DownloaderFileInformation::Status::downloaded;

        ASSERT_EQ(downloaderStorage->addFile(fileInfo),
            DistributedFileDownloader::ErrorCode::noError);
    }

    const auto& fileInfo = downloaderStorage->fileInformation(testFileName);
    ASSERT_EQ(fileInfo.status, DownloaderFileInformation::Status::downloaded);
    ASSERT_EQ(fileInfo.size, kTestFileSize);
    ASSERT_TRUE(fileInfo.md5 == testFileMd5);
    ASSERT_TRUE(fileInfo.chunkSize > 0);
    ASSERT_EQ(fileInfo.downloadedChunks.size(),
        Storage::calculateChunkCount(fileInfo.size, fileInfo.chunkSize));
}

TEST_F(DistributedFileDownloaderStorageTest, addDownloadedFileAsNotDownloaded)
{
    ASSERT_TRUE(createDefaultTestFile());

    {
        DownloaderFileInformation fileInfo(testFileName);
        fileInfo.md5 = testFileMd5;

        ASSERT_EQ(downloaderStorage->addFile(fileInfo),
            DistributedFileDownloader::ErrorCode::noError);
    }

    // Valid file is already exists, so file should became downloaded.
    const auto& fileInfo = downloaderStorage->fileInformation(testFileName);
    ASSERT_EQ(fileInfo.status, DownloaderFileInformation::Status::downloaded);
    ASSERT_EQ(fileInfo.size, kTestFileSize);
    ASSERT_TRUE(fileInfo.md5 == testFileMd5);
    ASSERT_TRUE(fileInfo.chunkSize > 0);
    ASSERT_EQ(fileInfo.downloadedChunks.size(),
        Storage::calculateChunkCount(fileInfo.size, fileInfo.chunkSize));
}

TEST_F(DistributedFileDownloaderStorageTest, addFileWithWrongChecksum)
{
    ASSERT_TRUE(createDefaultTestFile());

    {
        DownloaderFileInformation fileInfo(testFileName);
        fileInfo.status = DownloaderFileInformation::Status::downloaded;
        fileInfo.md5 = "invalid_md5";

        ASSERT_EQ(downloaderStorage->addFile(fileInfo),
            DistributedFileDownloader::ErrorCode::invalidChecksum);
    }

    const auto& fileInfo = downloaderStorage->fileInformation(testFileName);
    ASSERT_EQ(fileInfo.status, DownloaderFileInformation::Status::notFound);
}

TEST_F(DistributedFileDownloaderStorageTest, addFileForUpload)
{
    {
        DownloaderFileInformation fileInfo(testFileName);
        fileInfo.status = DownloaderFileInformation::Status::uploading;

        ASSERT_EQ(downloaderStorage->addFile(fileInfo),
            DistributedFileDownloader::ErrorCode::invalidFileSize);

        fileInfo.size = kTestFileSize;

        ASSERT_EQ(downloaderStorage->addFile(fileInfo),
            DistributedFileDownloader::ErrorCode::invalidChecksum);

        fileInfo.md5 = "md5";

        ASSERT_EQ(downloaderStorage->addFile(fileInfo),
            DistributedFileDownloader::ErrorCode::noError);
    }

    const auto& fileInfo = downloaderStorage->fileInformation(testFileName);
    ASSERT_EQ(fileInfo.status, DownloaderFileInformation::Status::uploading);
}

TEST_F(DistributedFileDownloaderStorageTest, fileDuplicate)
{
    ASSERT_EQ(downloaderStorage->addFile(testFileName),
        DistributedFileDownloader::ErrorCode::noError);

    ASSERT_EQ(downloaderStorage->addFile(testFileName),
        DistributedFileDownloader::ErrorCode::fileAlreadyExists);
}

TEST_F(DistributedFileDownloaderStorageTest, fileDeletion)
{
    ASSERT_TRUE(createTestFile(testFileName, 0));

    ASSERT_EQ(downloaderStorage->deleteFile(testFileName),
        DistributedFileDownloader::ErrorCode::fileDoesNotExist);

    // Delete download storing the data.
    ASSERT_EQ(downloaderStorage->addFile(testFileName),
        DistributedFileDownloader::ErrorCode::noError);
    ASSERT_EQ(downloaderStorage->deleteFile(testFileName, false),
        DistributedFileDownloader::ErrorCode::noError);
    ASSERT_TRUE(QFile::exists(testFilePath));

    // Delete download with its data.
    ASSERT_EQ(downloaderStorage->addFile(testFileName),
        DistributedFileDownloader::ErrorCode::noError);
    ASSERT_EQ(downloaderStorage->deleteFile(testFileName),
        DistributedFileDownloader::ErrorCode::noError);
    ASSERT_FALSE(QFile::exists(testFilePath));
}

TEST_F(DistributedFileDownloaderStorageTest, findDownloadsForDownloadedFiles)
{
    ASSERT_TRUE(createDefaultTestFile());

    {
        DownloaderFileInformation fileInfo(testFileName);
        fileInfo.status = DownloaderFileInformation::Status::downloaded;

        ASSERT_EQ(this->downloaderStorage->addFile(fileInfo),
            DistributedFileDownloader::ErrorCode::noError);
    }

    Storage downloaderStorage(workingDirectory);

    const auto& fileInfo = downloaderStorage.fileInformation(testFileName);
    ASSERT_EQ(fileInfo.status, DownloaderFileInformation::Status::downloaded);
    ASSERT_EQ(fileInfo.size, kTestFileSize);
    ASSERT_TRUE(fileInfo.md5 == testFileMd5);
    ASSERT_TRUE(fileInfo.md5 == Storage::calculateMd5(testFilePath));
    ASSERT_TRUE(fileInfo.chunkSize > 0);
    ASSERT_EQ(fileInfo.downloadedChunks.size(),
        Storage::calculateChunkCount(fileInfo.size, fileInfo.chunkSize));
}

TEST_F(DistributedFileDownloaderStorageTest, findDownloadsForNewFiles)
{
    ASSERT_EQ(this->downloaderStorage->addFile(testFileName),
        DistributedFileDownloader::ErrorCode::noError);

    Storage downloaderStorage(workingDirectory);

    const auto& fileInfo = downloaderStorage.fileInformation(testFileName);
    ASSERT_EQ(fileInfo.status, DownloaderFileInformation::Status::downloading);
}

TEST_F(DistributedFileDownloaderStorageTest, findDownloadsForFilesInProgress)
{
    ASSERT_TRUE(createDefaultTestFile());

    const auto targetFileName = testFileName + ".new";
    const auto targetFilePath = workingDirectory.absoluteFilePath(targetFileName);
    QByteArray md5BeforeFind;

    {
        DownloaderFileInformation fileInfo(testFileName);
        fileInfo.status = DownloaderFileInformation::Status::downloaded;

        ASSERT_EQ(this->downloaderStorage->addFile(fileInfo),
            DistributedFileDownloader::ErrorCode::noError);

        fileInfo = DownloaderFileInformation(targetFileName);
        fileInfo.md5 = testFileMd5;
        fileInfo.size = kTestFileSize;

        ASSERT_EQ(this->downloaderStorage->addFile(fileInfo),
            DistributedFileDownloader::ErrorCode::noError);

        QByteArray buffer;

        ASSERT_EQ(this->downloaderStorage->readFileChunk(testFileName, 0, buffer),
            DistributedFileDownloader::ErrorCode::noError);
        ASSERT_EQ(this->downloaderStorage->writeFileChunk(targetFileName, 0, buffer),
            DistributedFileDownloader::ErrorCode::noError);

        md5BeforeFind = Storage::calculateMd5(targetFilePath);
    }

    Storage downloaderStorage(workingDirectory);

    const auto& fileInfo = downloaderStorage.fileInformation(targetFileName);
    ASSERT_EQ(fileInfo.status, DownloaderFileInformation::Status::downloading);
    ASSERT_TRUE(md5BeforeFind == Storage::calculateMd5(targetFilePath));
}

TEST_F(DistributedFileDownloaderStorageTest, findCorruptedFiles)
{
    {
        DownloaderFileInformation fileInfo(testFileName);
        fileInfo.size = 1;
        fileInfo.md5 = "invalid_md5";

        ASSERT_EQ(this->downloaderStorage->addFile(fileInfo),
            DistributedFileDownloader::ErrorCode::noError);

        const QByteArray buffer("_");
        ASSERT_EQ(this->downloaderStorage->writeFileChunk(testFileName, 0, buffer),
            DistributedFileDownloader::ErrorCode::noError);

        fileInfo = downloaderStorage->fileInformation(testFileName);
        ASSERT_EQ(fileInfo.status, DownloaderFileInformation::Status::corrupted);
    }

    Storage downloaderStorage(workingDirectory);

    const auto& fileInfo = downloaderStorage.fileInformation(testFileName);
    ASSERT_EQ(fileInfo.status, DownloaderFileInformation::Status::corrupted);
}

TEST_F(DistributedFileDownloaderStorageTest, findFileAfterDeletion)
{
    ASSERT_TRUE(createTestFile(testFileName, 0));

    ASSERT_EQ(this->downloaderStorage->addFile(testFileName),
        DistributedFileDownloader::ErrorCode::noError);
    ASSERT_EQ(this->downloaderStorage->deleteFile(testFileName),
        DistributedFileDownloader::ErrorCode::noError);

    Storage downloaderStorage(workingDirectory);

    const auto& fileInfo = downloaderStorage.fileInformation(testFileName);
    ASSERT_EQ(fileInfo.status, DownloaderFileInformation::Status::notFound);
}

TEST_F(DistributedFileDownloaderStorageTest, chunkOperations)
{
    ASSERT_TRUE(createDefaultTestFile());

    const auto targetFileName = testFileName + ".new";

    {
        DownloaderFileInformation fileInfo(testFileName);
        fileInfo.status = DownloaderFileInformation::Status::downloaded;

        ASSERT_EQ(downloaderStorage->addFile(fileInfo),
            DistributedFileDownloader::ErrorCode::noError);

        DownloaderFileInformation targetFileInfo(targetFileName);
        targetFileInfo.md5 = testFileMd5;
        targetFileInfo.size = kTestFileSize;

        ASSERT_EQ(downloaderStorage->addFile(targetFileInfo),
            DistributedFileDownloader::ErrorCode::noError);
    }

    const auto& fileInfo = downloaderStorage->fileInformation(testFileName);

    for (int i = 0; i < fileInfo.downloadedChunks.size(); ++i)
    {
        QByteArray buffer;

        ASSERT_EQ(downloaderStorage->readFileChunk(testFileName, i, buffer),
            DistributedFileDownloader::ErrorCode::noError);
        ASSERT_EQ(downloaderStorage->writeFileChunk(targetFileName, i, buffer),
            DistributedFileDownloader::ErrorCode::noError);
    }

    const auto& targetFileInfo = downloaderStorage->fileInformation(targetFileName);
    ASSERT_EQ(targetFileInfo.status, DownloaderFileInformation::Status::downloaded);

    const auto targetMd5 = Storage::calculateMd5(
        workingDirectory.absoluteFilePath(targetFileInfo.name));
    ASSERT_TRUE(testFileMd5 == targetMd5);
}

TEST_F(DistributedFileDownloaderStorageTest, readByInvalidChunkIndex)
{
    ASSERT_TRUE(createDefaultTestFile());

    DownloaderFileInformation fileInformation(testFileName);
    fileInformation.status = DownloaderFileInformation::Status::downloaded;

    ASSERT_EQ(downloaderStorage->addFile(fileInformation),
        DistributedFileDownloader::ErrorCode::noError);

    QByteArray buffer;
    ASSERT_EQ(downloaderStorage->readFileChunk(testFileName, 42, buffer),
        DistributedFileDownloader::ErrorCode::invalidChunkIndex);
}

TEST_F(DistributedFileDownloaderStorageTest, writeInvalidChunks)
{
    ASSERT_TRUE(createDefaultTestFile());

    DownloaderFileInformation fileInformation(testFileName);
    fileInformation.size = 1;

    ASSERT_EQ(downloaderStorage->addFile(fileInformation),
        DistributedFileDownloader::ErrorCode::noError);

    QByteArray buffer;

    ASSERT_EQ(downloaderStorage->writeFileChunk(testFileName, 42, buffer),
        DistributedFileDownloader::ErrorCode::invalidChunkIndex);

    buffer = "some_bytes";
    ASSERT_EQ(downloaderStorage->writeFileChunk(testFileName, 0, buffer),
        DistributedFileDownloader::ErrorCode::invalidChunkSize);
}

TEST_F(DistributedFileDownloaderStorageTest, writeToDownloadedFile)
{
    ASSERT_TRUE(createDefaultTestFile());

    DownloaderFileInformation fileInformation(testFileName);
    fileInformation.status = DownloaderFileInformation::Status::downloaded;

    ASSERT_EQ(downloaderStorage->addFile(fileInformation),
        DistributedFileDownloader::ErrorCode::noError);

    QByteArray buffer;
    ASSERT_EQ(downloaderStorage->writeFileChunk(testFileName, 0, buffer),
        DistributedFileDownloader::ErrorCode::ioError);
}

TEST_F(DistributedFileDownloaderStorageTest, fileCorruptionDuringDownload)
{
    {
        DownloaderFileInformation fileInfo(testFileName);
        fileInfo.md5 = "md5_corrupted";
        fileInfo.size = 1;

        ASSERT_EQ(downloaderStorage->addFile(fileInfo),
            DistributedFileDownloader::ErrorCode::noError);
    }

    ASSERT_EQ(downloaderStorage->writeFileChunk(testFileName, 0, "1"),
        DistributedFileDownloader::ErrorCode::noError);

    const auto& fileInformation = downloaderStorage->fileInformation(testFileName);
    ASSERT_EQ(fileInformation.status, DownloaderFileInformation::Status::corrupted);
}

TEST_F(DistributedFileDownloaderStorageTest, getChecksums)
{
    ASSERT_TRUE(createDefaultTestFile());

    DownloaderFileInformation fileInformation(testFileName);
    fileInformation.status = DownloaderFileInformation::Status::downloaded;

    ASSERT_EQ(downloaderStorage->addFile(fileInformation),
        DistributedFileDownloader::ErrorCode::noError);

    const auto& fileInfo = downloaderStorage->fileInformation(testFileName);

    const auto checksums = downloaderStorage->getChunkChecksums(testFileName);
    const auto chunkCount = Storage::calculateChunkCount(
        fileInfo.size, fileInfo.chunkSize);
    ASSERT_EQ(checksums.size(), chunkCount);
}

TEST_F(DistributedFileDownloaderStorageTest, getChecksumsAfterDownload)
{
    ASSERT_TRUE(createTestFile(testFileName, 1024));

    DownloaderFileInformation fileInfo(testFileName);
    fileInfo.status = DownloaderFileInformation::Status::downloaded;

    ASSERT_EQ(downloaderStorage->addFile(fileInfo),
        DistributedFileDownloader::ErrorCode::noError);

    fileInfo = downloaderStorage->fileInformation(testFileName);

    QByteArray data;
    ASSERT_EQ(downloaderStorage->readFileChunk(testFileName, 0, data),
        DistributedFileDownloader::ErrorCode::noError);

    DownloaderFileInformation targetFileInfo(testFileName + ".new");
    targetFileInfo.size = fileInfo.size;
    targetFileInfo.md5 = fileInfo.md5;
    ASSERT_EQ(downloaderStorage->addFile(targetFileInfo),
        DistributedFileDownloader::ErrorCode::noError);

    ASSERT_EQ(downloaderStorage->writeFileChunk(targetFileInfo.name, 0, data),
        DistributedFileDownloader::ErrorCode::noError);

    const auto checksums = downloaderStorage->getChunkChecksums(targetFileInfo.name);
    ASSERT_EQ(checksums.size(), 1);
    ASSERT_FALSE(checksums[0].isEmpty());
}

} // namespace test
} // namespace common
} // namespace vms
} // namespace nx
