#include <gtest/gtest.h>

#include <QtCore/QDir>
#include <QtCore/QFile>

#include <nx/vms/common/distributed_file_downloader/private/storage.h>

#include <test_setup.h>

#include "utils.h"

namespace nx {
namespace vms {
namespace common {
namespace distributed_file_downloader {
namespace test {

namespace {

static const QString kTestFileName("test.dat");
static const qint64 kTestFileSize = 1024 * 1024 * 4 + 42;

} // namespace

class DistributedFileDownloaderStorageTest: public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
        workingDirectory =
            QDir(TestSetup::getTemporaryDirectoryPath()).absoluteFilePath("storage_ut");
        workingDirectory.removeRecursively();
        NX_ASSERT(QDir().mkdir(workingDirectory.absolutePath()));

        testFileName = kTestFileName;
        testFilePath = workingDirectory.absoluteFilePath(testFileName);

        downloaderStorage.reset(new Storage(workingDirectory));
    }

    virtual void TearDown() override
    {
        downloaderStorage.reset();
        NX_ASSERT(workingDirectory.removeRecursively());
    }

    void createTestFile(const QString& fileName, qint64 size)
    {
        utils::createTestFile(workingDirectory.absoluteFilePath(fileName), size);

        testFileMd5 = Storage::calculateMd5(testFilePath);
        NX_ASSERT(!testFileMd5.isEmpty());
    }

    void createDefaultTestFile()
    {
        createTestFile(testFileName, kTestFileSize);
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
    ASSERT_EQ(fileInfo.status, FileInformation::Status::notFound);

    QByteArray buffer;

    ASSERT_EQ(downloaderStorage->readFileChunk(testFileName, 0, buffer),
        ErrorCode::fileDoesNotExist);

    ASSERT_EQ(downloaderStorage->writeFileChunk(testFileName, 0, buffer),
        ErrorCode::fileDoesNotExist);

    ASSERT_EQ(downloaderStorage->deleteFile(testFileName),
        ErrorCode::fileDoesNotExist);
}

TEST_F(DistributedFileDownloaderStorageTest, addNewFileWithoutInformation)
{
    ASSERT_EQ(downloaderStorage->addFile(testFileName),
        ErrorCode::noError);

    const auto& fileInfo = downloaderStorage->fileInformation(testFileName);

    ASSERT_EQ(fileInfo.status, FileInformation::Status::downloading);
    ASSERT_EQ(fileInfo.size, -1);
    ASSERT_TRUE(fileInfo.md5.isEmpty());
}

TEST_F(DistributedFileDownloaderStorageTest, addNewFile)
{
    const QByteArray md5("md5_test");

    {
        FileInformation fileInfo(testFileName);
        fileInfo.size = kTestFileSize;
        fileInfo.md5 = md5;

        ASSERT_EQ(downloaderStorage->addFile(fileInfo),
            ErrorCode::noError);
    }

    const auto& fileInfo = downloaderStorage->fileInformation(testFileName);

    ASSERT_EQ(fileInfo.status, FileInformation::Status::downloading);
    ASSERT_EQ(fileInfo.size, kTestFileSize);
    ASSERT_TRUE(fileInfo.md5 == md5);
    ASSERT_FALSE(fileInfo.downloadedChunks.isEmpty());
}

TEST_F(DistributedFileDownloaderStorageTest, addDownloadedFile)
{
    createDefaultTestFile();

    {
        FileInformation fileInfo(testFileName);
        fileInfo.status = FileInformation::Status::downloaded;

        ASSERT_EQ(downloaderStorage->addFile(fileInfo),
            ErrorCode::noError);
    }

    const auto& fileInfo = downloaderStorage->fileInformation(testFileName);
    ASSERT_EQ(fileInfo.status, FileInformation::Status::downloaded);
    ASSERT_EQ(fileInfo.size, kTestFileSize);
    ASSERT_TRUE(fileInfo.md5 == testFileMd5);
    ASSERT_TRUE(fileInfo.chunkSize > 0);
    ASSERT_EQ(fileInfo.downloadedChunks.size(),
        Storage::calculateChunkCount(fileInfo.size, fileInfo.chunkSize));
}

TEST_F(DistributedFileDownloaderStorageTest, addDownloadedFileAsNotDownloaded)
{
    createDefaultTestFile();

    {
        FileInformation fileInfo(testFileName);
        fileInfo.md5 = testFileMd5;

        ASSERT_EQ(downloaderStorage->addFile(fileInfo),
            ErrorCode::noError);
    }

    // Valid file is already exists, so file should became downloaded.
    const auto& fileInfo = downloaderStorage->fileInformation(testFileName);
    ASSERT_EQ(fileInfo.status, FileInformation::Status::downloaded);
    ASSERT_EQ(fileInfo.size, kTestFileSize);
    ASSERT_TRUE(fileInfo.md5 == testFileMd5);
    ASSERT_TRUE(fileInfo.chunkSize > 0);
    ASSERT_EQ(fileInfo.downloadedChunks.size(),
        Storage::calculateChunkCount(fileInfo.size, fileInfo.chunkSize));
}

TEST_F(DistributedFileDownloaderStorageTest, addFileWithWrongChecksum)
{
    createDefaultTestFile();

    {
        FileInformation fileInfo(testFileName);
        fileInfo.status = FileInformation::Status::downloaded;
        fileInfo.md5 = "invalid_md5";

        ASSERT_EQ(downloaderStorage->addFile(fileInfo),
            ErrorCode::invalidChecksum);
    }

    const auto& fileInfo = downloaderStorage->fileInformation(testFileName);
    ASSERT_EQ(fileInfo.status, FileInformation::Status::notFound);
}

TEST_F(DistributedFileDownloaderStorageTest, addFileForUpload)
{
    {
        FileInformation fileInfo(testFileName);
        fileInfo.status = FileInformation::Status::uploading;

        ASSERT_EQ(downloaderStorage->addFile(fileInfo),
            ErrorCode::invalidFileSize);

        fileInfo.size = kTestFileSize;

        ASSERT_EQ(downloaderStorage->addFile(fileInfo),
            ErrorCode::invalidChecksum);

        fileInfo.md5 = "md5";

        ASSERT_EQ(downloaderStorage->addFile(fileInfo),
            ErrorCode::noError);
    }

    const auto& fileInfo = downloaderStorage->fileInformation(testFileName);
    ASSERT_EQ(fileInfo.status, FileInformation::Status::uploading);
}

TEST_F(DistributedFileDownloaderStorageTest, fileNameWithSubdirectories)
{
    const QString fileName("one/two/three/" + testFileName);

    ASSERT_EQ(downloaderStorage->addFile(fileName),
        ErrorCode::noError);

    const auto filePath = downloaderStorage->filePath(fileName);

    ASSERT_EQ(filePath, workingDirectory.absoluteFilePath(fileName));
    ASSERT_TRUE(QFile::exists(filePath));

    ASSERT_EQ(downloaderStorage->deleteFile(fileName),
        ErrorCode::noError);

    ASSERT_FALSE(workingDirectory.exists("one"));
}

TEST_F(DistributedFileDownloaderStorageTest, fileDuplicate)
{
    ASSERT_EQ(downloaderStorage->addFile(testFileName),
        ErrorCode::noError);

    ASSERT_EQ(downloaderStorage->addFile(testFileName),
        ErrorCode::fileAlreadyExists);
}

TEST_F(DistributedFileDownloaderStorageTest, fileDeletion)
{
    createTestFile(testFileName, 0);

    ASSERT_EQ(downloaderStorage->deleteFile(testFileName),
        ErrorCode::fileDoesNotExist);

    // Delete download storing the data.
    ASSERT_EQ(downloaderStorage->addFile(testFileName),
        ErrorCode::noError);
    ASSERT_EQ(downloaderStorage->deleteFile(testFileName, false),
        ErrorCode::noError);
    ASSERT_TRUE(QFile::exists(testFilePath));

    // Delete download with its data.
    ASSERT_EQ(downloaderStorage->addFile(testFileName),
        ErrorCode::noError);
    ASSERT_EQ(downloaderStorage->deleteFile(testFileName),
        ErrorCode::noError);
    ASSERT_FALSE(QFile::exists(testFilePath));
}

TEST_F(DistributedFileDownloaderStorageTest, updateEmptyFile)
{
    ASSERT_EQ(downloaderStorage->addFile(testFileName),
        ErrorCode::noError);

    ASSERT_EQ(downloaderStorage->updateFileInformation(
        testFileName + ".notfound", kTestFileSize, QByteArray()),
        ErrorCode::fileDoesNotExist);

    ASSERT_EQ(downloaderStorage->updateFileInformation(testFileName, kTestFileSize, QByteArray()),
        ErrorCode::noError);
    ASSERT_EQ(kTestFileSize, Storage::calculateFileSize(testFilePath));

    ASSERT_EQ(downloaderStorage->updateFileInformation(testFileName, kTestFileSize, "md5_test"),
        ErrorCode::noError);

    const auto& fileInfo = downloaderStorage->fileInformation(testFileName);
    ASSERT_EQ(fileInfo.size, kTestFileSize);
    ASSERT_EQ(fileInfo.md5, "md5_test");
}

TEST_F(DistributedFileDownloaderStorageTest, updateDownloadedFile)
{
    createDefaultTestFile();

    FileInformation fileInfo(testFileName);
    fileInfo.status = FileInformation::Status::downloaded;
    ASSERT_EQ(downloaderStorage->addFile(fileInfo),
        ErrorCode::noError);

    ASSERT_EQ(downloaderStorage->updateFileInformation(testFileName, kTestFileSize, "md5_test"),
        ErrorCode::fileAlreadyDownloaded);
}

TEST_F(DistributedFileDownloaderStorageTest, updateCorruptedFile)
{
    createTestFile(testFileName, 1);

    FileInformation fileInfo(testFileName);
    fileInfo.size = 1;
    fileInfo.md5 = "invalid_md5";

    ASSERT_EQ(downloaderStorage->addFile(fileInfo),
        ErrorCode::noError);

    QByteArray data;

    {
        QFile file(testFilePath);
        file.open(QFile::ReadOnly);
        data = file.readAll();
        file.close();
    }

    ASSERT_EQ(downloaderStorage->writeFileChunk(testFileName, 0, data),
        ErrorCode::noError);

    {
        const auto& fileInfo = downloaderStorage->fileInformation(testFileName);
        ASSERT_EQ(fileInfo.status, FileInformation::Status::corrupted);
    }

    ASSERT_EQ(downloaderStorage->updateFileInformation(testFileName, -1, testFileMd5),
        ErrorCode::noError);

    {
        const auto& fileInfo = downloaderStorage->fileInformation(testFileName);
        ASSERT_EQ(fileInfo.status, FileInformation::Status::downloaded);
    }
}

TEST_F(DistributedFileDownloaderStorageTest, findDownloadsForDownloadedFiles)
{
    createDefaultTestFile();

    {
        FileInformation fileInfo(testFileName);
        fileInfo.status = FileInformation::Status::downloaded;

        ASSERT_EQ(this->downloaderStorage->addFile(fileInfo),
            ErrorCode::noError);
    }

    Storage downloaderStorage(workingDirectory);

    const auto& fileInfo = downloaderStorage.fileInformation(testFileName);
    ASSERT_EQ(fileInfo.status, FileInformation::Status::downloaded);
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
        ErrorCode::noError);

    Storage downloaderStorage(workingDirectory);

    const auto& fileInfo = downloaderStorage.fileInformation(testFileName);
    ASSERT_EQ(fileInfo.status, FileInformation::Status::downloading);
}

TEST_F(DistributedFileDownloaderStorageTest, findDownloadsForFilesInProgress)
{
    createDefaultTestFile();

    const auto targetFileName = testFileName + ".new";
    const auto targetFilePath = workingDirectory.absoluteFilePath(targetFileName);
    QByteArray md5BeforeFind;

    {
        FileInformation fileInfo(testFileName);
        fileInfo.status = FileInformation::Status::downloaded;

        ASSERT_EQ(this->downloaderStorage->addFile(fileInfo),
            ErrorCode::noError);

        fileInfo = FileInformation(targetFileName);
        fileInfo.md5 = testFileMd5;
        fileInfo.size = kTestFileSize;

        ASSERT_EQ(this->downloaderStorage->addFile(fileInfo),
            ErrorCode::noError);

        QByteArray buffer;

        ASSERT_EQ(this->downloaderStorage->readFileChunk(testFileName, 0, buffer),
            ErrorCode::noError);
        ASSERT_EQ(this->downloaderStorage->writeFileChunk(targetFileName, 0, buffer),
            ErrorCode::noError);

        md5BeforeFind = Storage::calculateMd5(targetFilePath);
    }

    Storage downloaderStorage(workingDirectory);

    const auto& fileInfo = downloaderStorage.fileInformation(targetFileName);
    ASSERT_EQ(fileInfo.status, FileInformation::Status::downloading);
    ASSERT_TRUE(md5BeforeFind == Storage::calculateMd5(targetFilePath));
}

TEST_F(DistributedFileDownloaderStorageTest, findCorruptedFiles)
{
    {
        FileInformation fileInfo(testFileName);
        fileInfo.size = 1;
        fileInfo.md5 = "invalid_md5";

        ASSERT_EQ(this->downloaderStorage->addFile(fileInfo),
            ErrorCode::noError);

        const QByteArray buffer("_");
        ASSERT_EQ(this->downloaderStorage->writeFileChunk(testFileName, 0, buffer),
            ErrorCode::noError);

        fileInfo = downloaderStorage->fileInformation(testFileName);
        ASSERT_EQ(fileInfo.status, FileInformation::Status::corrupted);
    }

    Storage downloaderStorage(workingDirectory);

    const auto& fileInfo = downloaderStorage.fileInformation(testFileName);
    ASSERT_EQ(fileInfo.status, FileInformation::Status::corrupted);
}

TEST_F(DistributedFileDownloaderStorageTest, findFileAfterDeletion)
{
    createTestFile(testFileName, 0);

    ASSERT_EQ(this->downloaderStorage->addFile(testFileName),
        ErrorCode::noError);
    ASSERT_EQ(this->downloaderStorage->deleteFile(testFileName),
        ErrorCode::noError);

    Storage downloaderStorage(workingDirectory);

    const auto& fileInfo = downloaderStorage.fileInformation(testFileName);
    ASSERT_EQ(fileInfo.status, FileInformation::Status::notFound);
}

TEST_F(DistributedFileDownloaderStorageTest, chunkOperations)
{
    createDefaultTestFile();

    const auto targetFileName = testFileName + ".new";

    {
        FileInformation fileInfo(testFileName);
        fileInfo.status = FileInformation::Status::downloaded;

        ASSERT_EQ(downloaderStorage->addFile(fileInfo),
            ErrorCode::noError);

        FileInformation targetFileInfo(targetFileName);
        targetFileInfo.md5 = testFileMd5;
        targetFileInfo.size = kTestFileSize;

        ASSERT_EQ(downloaderStorage->addFile(targetFileInfo),
            ErrorCode::noError);
    }

    const auto& fileInfo = downloaderStorage->fileInformation(testFileName);

    for (int i = 0; i < fileInfo.downloadedChunks.size(); ++i)
    {
        QByteArray buffer;

        ASSERT_EQ(downloaderStorage->readFileChunk(testFileName, i, buffer),
            ErrorCode::noError);
        ASSERT_EQ(downloaderStorage->writeFileChunk(targetFileName, i, buffer),
            ErrorCode::noError);
    }

    const auto& targetFileInfo = downloaderStorage->fileInformation(targetFileName);
    ASSERT_EQ(targetFileInfo.status, FileInformation::Status::downloaded);

    const auto targetMd5 = Storage::calculateMd5(
        workingDirectory.absoluteFilePath(targetFileInfo.name));
    ASSERT_TRUE(testFileMd5 == targetMd5);
}

TEST_F(DistributedFileDownloaderStorageTest, readByInvalidChunkIndex)
{
    createDefaultTestFile();

    FileInformation fileInformation(testFileName);
    fileInformation.status = FileInformation::Status::downloaded;

    ASSERT_EQ(downloaderStorage->addFile(fileInformation),
        ErrorCode::noError);

    QByteArray buffer;
    ASSERT_EQ(downloaderStorage->readFileChunk(testFileName, 42, buffer),
        ErrorCode::invalidChunkIndex);
}

TEST_F(DistributedFileDownloaderStorageTest, writeInvalidChunks)
{
    createDefaultTestFile();

    FileInformation fileInformation(testFileName);
    fileInformation.size = 1;

    ASSERT_EQ(downloaderStorage->addFile(fileInformation),
        ErrorCode::noError);

    QByteArray buffer;

    ASSERT_EQ(downloaderStorage->writeFileChunk(testFileName, 42, buffer),
        ErrorCode::invalidChunkIndex);

    buffer = "some_bytes";
    ASSERT_EQ(downloaderStorage->writeFileChunk(testFileName, 0, buffer),
        ErrorCode::invalidChunkSize);
}

TEST_F(DistributedFileDownloaderStorageTest, writeToDownloadedFile)
{
    createDefaultTestFile();

    FileInformation fileInformation(testFileName);
    fileInformation.status = FileInformation::Status::downloaded;

    ASSERT_EQ(downloaderStorage->addFile(fileInformation),
        ErrorCode::noError);

    QByteArray buffer;
    ASSERT_EQ(downloaderStorage->writeFileChunk(testFileName, 0, buffer),
        ErrorCode::ioError);
}

TEST_F(DistributedFileDownloaderStorageTest, fileCorruptionDuringDownload)
{
    {
        FileInformation fileInfo(testFileName);
        fileInfo.md5 = "md5_corrupted";
        fileInfo.size = 1;

        ASSERT_EQ(downloaderStorage->addFile(fileInfo),
            ErrorCode::noError);
    }

    ASSERT_EQ(downloaderStorage->writeFileChunk(testFileName, 0, "1"),
        ErrorCode::noError);

    const auto& fileInformation = downloaderStorage->fileInformation(testFileName);
    ASSERT_EQ(fileInformation.status, FileInformation::Status::corrupted);
}

TEST_F(DistributedFileDownloaderStorageTest, getChecksums)
{
    createDefaultTestFile();

    FileInformation fileInformation(testFileName);
    fileInformation.status = FileInformation::Status::downloaded;

    ASSERT_EQ(downloaderStorage->addFile(fileInformation),
        ErrorCode::noError);

    const auto& fileInfo = downloaderStorage->fileInformation(testFileName);

    const auto checksums = downloaderStorage->getChunkChecksums(testFileName);
    const auto chunkCount = Storage::calculateChunkCount(
        fileInfo.size, fileInfo.chunkSize);
    ASSERT_EQ(checksums.size(), chunkCount);
}

TEST_F(DistributedFileDownloaderStorageTest, getChecksumsAfterDownload)
{
    createTestFile(testFileName, 1024);

    FileInformation fileInfo(testFileName);
    fileInfo.status = FileInformation::Status::downloaded;

    ASSERT_EQ(downloaderStorage->addFile(fileInfo),
        ErrorCode::noError);

    fileInfo = downloaderStorage->fileInformation(testFileName);

    QByteArray data;
    ASSERT_EQ(downloaderStorage->readFileChunk(testFileName, 0, data),
        ErrorCode::noError);

    FileInformation targetFileInfo(testFileName + ".new");
    targetFileInfo.size = fileInfo.size;
    targetFileInfo.md5 = fileInfo.md5;
    ASSERT_EQ(downloaderStorage->addFile(targetFileInfo),
        ErrorCode::noError);

    ASSERT_EQ(downloaderStorage->writeFileChunk(targetFileInfo.name, 0, data),
        ErrorCode::noError);

    const auto checksums = downloaderStorage->getChunkChecksums(targetFileInfo.name);
    ASSERT_EQ(checksums.size(), 1);
    ASSERT_FALSE(checksums[0].isEmpty());
}

TEST_F(DistributedFileDownloaderStorageTest, setChecksumsToCorruptedFile)
{
    const int kChunkSize = 1024 * 1024;

    createDefaultTestFile();
    const auto checksums = Storage::calculateChecksums(testFilePath, kChunkSize);

    QFile file(testFilePath);
    if (!file.open(QFile::ReadWrite))
    {
        NX_ASSERT(false, "Cannot open test file.");
        return;
    }
    file.seek(0);
    file.write(QByteArray(kChunkSize, '\0'));
    file.close();

    FileInformation fileInfo(testFileName);
    fileInfo.status = FileInformation::Status::corrupted;
    fileInfo.size = kTestFileSize;
    fileInfo.md5 = testFileMd5;
    fileInfo.chunkSize = kChunkSize;
    fileInfo.downloadedChunks.fill(true, Storage::calculateChunkCount(kTestFileSize, kChunkSize));

    ASSERT_EQ(downloaderStorage->addFile(fileInfo),
        ErrorCode::noError);

    ASSERT_EQ(downloaderStorage->setChunkChecksums(testFileName, QVector<QByteArray>(1)),
        ErrorCode::invalidChecksum);

    ASSERT_EQ(downloaderStorage->setChunkChecksums(testFileName, checksums),
        ErrorCode::noError);

    fileInfo = downloaderStorage->fileInformation(testFileName);
    ASSERT_FALSE(fileInfo.downloadedChunks[0]);
    ASSERT_EQ(fileInfo.downloadedChunks.count(true), fileInfo.downloadedChunks.size() - 1);
    ASSERT_EQ(fileInfo.status, FileInformation::Status::downloading);
}

TEST_F(DistributedFileDownloaderStorageTest, setChecksumsToDownloadedFile)
{
    createDefaultTestFile();

    FileInformation fileInfo(testFileName);
    fileInfo.status = FileInformation::Status::downloaded;

    ASSERT_EQ(downloaderStorage->addFile(fileInfo),
        ErrorCode::noError);

    fileInfo = downloaderStorage->fileInformation(testFileName);

    ASSERT_EQ(
        downloaderStorage->setChunkChecksums(
            testFileName, QVector<QByteArray>(fileInfo.downloadedChunks.size())),
        ErrorCode::fileAlreadyDownloaded);
}

} // namespace test
} // namespace distributed_file_downloader
} // namespace common
} // namespace vms
} // namespace nx
