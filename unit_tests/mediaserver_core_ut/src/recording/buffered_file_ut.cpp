/**********************************************************
* Dec 07, 2015
* rvasilenko
***********************************************************/
#include <QFile>
#include <vector>
#include <memory>

#include <gtest/gtest.h>
#include <utils/common/uuid.h>
#include <utils/fs/file.h>
#include <utils/common/buffered_file.h>
#include <recorder/recording_manager.h>
#include <server/server_globals.h>

#include <QtCore>

void doTestInternal(int systemFlags)
{
    const QByteArray kTestFileName("test_data1.bin");
    const QByteArray kTestPattern("1234567890");
    const int kPatternRepCnt = 2000 * 499;

    // write file

    QFile::remove(kTestFileName);

    std::unique_ptr<QBufferedFile> testFile(
        new QBufferedFile(
        std::shared_ptr<IQnFile>(new QnFile(kTestFileName)),
        1024*1024*2,
        1024*1024*4,
        QnUuid::createUuid()
        ));

    testFile->setSystemFlags(systemFlags);

    ASSERT_TRUE(testFile->open(QIODevice::WriteOnly));

    for (int i = 0; i < kPatternRepCnt; ++i)
        testFile->write(kTestPattern);

    testFile->seek(5);
    testFile->write(kTestPattern);
    testFile->seek(32768-5);
    testFile->write(kTestPattern);

    testFile->close();

    // read file

    QFile checkResultFile(kTestFileName);
    ASSERT_TRUE(checkResultFile.open(QIODevice::ReadOnly));

    ASSERT_EQ(checkResultFile.size(), kPatternRepCnt * kTestPattern.length());

    char buffer[10];
    checkResultFile.read(buffer, 5);
    ASSERT_EQ(QByteArray(buffer, 5), QByteArray("12345"));

    checkResultFile.read(buffer, 10);
    ASSERT_EQ(QByteArray(buffer, 10), kTestPattern);

    checkResultFile.read(buffer, 5);
    ASSERT_EQ(QByteArray(buffer, 5), QByteArray("67890"));

    int rep1 = 32768 / kTestPattern.length();
    rep1 -= 2; // we've already readed 20 bytes
    for (int i = 0; i < rep1; ++i) {
        checkResultFile.read(buffer, 10);
        ASSERT_EQ(QByteArray(buffer, 10), kTestPattern);
    }

    checkResultFile.read(buffer, 3);
    ASSERT_EQ(QByteArray(buffer, 3), QByteArray("123"));

    checkResultFile.read(buffer, 10);
    ASSERT_EQ(QByteArray(buffer, 10), kTestPattern);

    checkResultFile.read(buffer, 7);
    ASSERT_EQ(QByteArray(buffer, 7), QByteArray("4567890"));

    int rep2 = (checkResultFile.size() - checkResultFile.pos()) / kTestPattern.length();
    for (int i = 0; i < rep2; ++i) {
        checkResultFile.read(buffer, 10);
        ASSERT_EQ(QByteArray(buffer, 10), kTestPattern);
    }

    ASSERT_EQ(checkResultFile.pos(), checkResultFile.size());
}

TEST(BufferedFileWriter, noDirectIO)
{
    doTestInternal(0);
}

TEST(BufferedFileWriter, withDirectIO)
{
#ifdef Q_OS_WIN
    doTestInternal(FILE_FLAG_NO_BUFFERING);
#endif
}


class WriteBufferMultiplierManagerTest
    : public WriteBufferMultiplierManager
{
public:
    FileToRecType& getFileToRec()
    {
        return m_fileToRec;
    }
    RecToSizeType& getRecToSize()
    {
        return m_recToMult;
    }
};

TEST(BufferedFileWriter, AdaptiveBufferSize)
{
    QTemporaryDir tmpDir;
    struct FileTestData
    {
        size_t index;
        QString fileName;
        std::unique_ptr<QBufferedFile> file;
        QnUuid mediaDeviceId;
        FileTestData(
            size_t index,
            const QString& fileName,
            QBufferedFile* filePtr,
            const QnUuid& mediaDeviceId) 
            :
            index(index),
            fileName(fileName),
            file(filePtr),
            mediaDeviceId(mediaDeviceId)
        {}
        FileTestData(FileTestData&& other)
            :
            index(other.index),
            fileName(std::move(other.fileName)),
            file(std::move(other.file)),
            mediaDeviceId(other.mediaDeviceId)
        {}
        ~FileTestData()
        {
            if (file)
                file->close();
            file.reset();
            QFile::remove(fileName);
        }
    };
    WriteBufferMultiplierManagerTest bufferManager;
    const size_t kFileCount = 5;
    const int kIoBlockSize = 1024 * 1024 * 4;
    const int kFfmpegBufferSize = 1024;
    const QnUuid kWriterPoolId = QnUuid::createUuid();
    std::vector<FileTestData> files;
    files.reserve(kFileCount);
    for (size_t i = 0; i < kFileCount; ++i)
    {
        QString fileName = lit("%1/test_file_%2.bin")
            .arg(tmpDir.path())
            .arg(i);
        QBufferedFile* ioDevice = 
            new QBufferedFile(
                std::shared_ptr<IQnFile>(new QnFile(fileName)),
                kIoBlockSize,
                kFfmpegBufferSize,
                kWriterPoolId);
#ifdef Q_OS_WIN
        ioDevice->setSystemFlags(FILE_FLAG_NO_BUFFERING);
#endif
        ioDevice->open(QIODevice::WriteOnly);
        QnUuid mediaDeviceId = QnUuid::createUuid();
        files.emplace_back(i, fileName, ioDevice, mediaDeviceId);
        QObject::connect(
            (QBufferedFile*)ioDevice,
            &QBufferedFile::seekDetected,
            &bufferManager,
            &WriteBufferMultiplierManager::at_seekDetected,
            Qt::DirectConnection);
        QObject::connect(
            (QBufferedFile*)ioDevice,
            &QBufferedFile::fileClosed,
            &bufferManager,
            &WriteBufferMultiplierManager::at_fileClosed,
            Qt::DirectConnection);
        bufferManager.setFilePtr(
            (uintptr_t)ioDevice,
            QnServer::HiQualityCatalog,
            mediaDeviceId);
    }
    ASSERT_EQ(bufferManager.getFileToRec().size(), kFileCount);
    ASSERT_TRUE(bufferManager.getRecToSize().empty());
    // seek in memory range
    for (size_t i = 0; i < kFileCount; ++i)
    {
        files[i].file->write(QByteArray(kIoBlockSize + 1024, 'a'));
        files[i].file->seek(kIoBlockSize + 5);
        // to force a real seek
        files[i].file->write("b", 1);
        // seek back
        files[i].file->seek(kIoBlockSize + 1024 -1);
        files[i].file->write("b", 1);
        // No seeks in physical file should have been made so far
        ASSERT_TRUE(bufferManager.getRecToSize().empty());
    }
    // seek out of memory range
    for (size_t i = 0; i < kFileCount; ++i)
    {
        files[i].file->seek(kIoBlockSize - 5);
        // real seek attempt is made only on write
        files[i].file->write(QByteArray(3, 'b'));
        files[i].file->seek(kIoBlockSize + 1024 - 1);
        // to force a real seek
        files[i].file->write("b", 1);
        // Two seeks in physical file have been made.
        // Corresponding signals should has been sent.
        // recToSize data structure should contain correspondence
        // (catalog, deviceId) -> (2^(log2(prevBufferSize) + 1 + 1))*1024 that is 
        // (catalog, deviceId) -> 4096 in this case.
        ASSERT_EQ(bufferManager.getRecToSize().size(), i+1);
        ASSERT_EQ(
            bufferManager.getSizeForCam(
                QnServer::HiQualityCatalog,
                files[i].mediaDeviceId),
            4096);
    }
    // Now seeks within (fileEndPos - 4096, fileEndPos) range 
    // should not trigger physical seeks and therefore no seekDetected
    // signals should be emitted and recToSize data structure shouldn't be 
    // altered.
    for (size_t i = 0; i < kFileCount; ++i)
    {
        files[i].file->write(QByteArray(kIoBlockSize + 4096, 'a'));
        files[i].file->seek(kIoBlockSize*2 + 5);
        files[i].file->write(QByteArray(3, 'b'));
        files[i].file->seek(kIoBlockSize*2 + 4095 );
        files[i].file->write("b", 1);
        ASSERT_EQ(bufferManager.getRecToSize().size(), 5);
        ASSERT_EQ(
            bufferManager.getSizeForCam(
                QnServer::HiQualityCatalog,
                files[i].mediaDeviceId),
            4096);
    }
    // On QBufferedFile object destruction signal should be emitted
    // with a net effect of deleting corresponding record 
    // from fileToRec dataStructure
    for (size_t i = 0; i < kFileCount; ++i)
    {
        uintptr_t filePtr = (uintptr_t)files[i].file.get();
        files[i].file->close();
        files[i].file.reset();
        ASSERT_EQ(bufferManager.getFileToRec().size(), kFileCount - i - 1);
        auto fileRecIt = bufferManager.getFileToRec().find(filePtr);
        ASSERT_TRUE(fileRecIt == bufferManager.getFileToRec().end());
    }
}