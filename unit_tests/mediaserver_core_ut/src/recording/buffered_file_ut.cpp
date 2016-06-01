/**********************************************************
* Dec 07, 2015
* rvasilenko
***********************************************************/

#include <QFile>

#include <gtest/gtest.h>
#include <nx/utils/uuid.h>
#include <utils/fs/file.h>
#include <utils/common/buffered_file.h>
#include "../utils.h"

void doTestInternal(int systemFlags)
{
    nx::ut::utils::WorkDirResource workDir;
    ASSERT_TRUE((bool)workDir.getDirName());

    const QString kTestFileName = QDir(*workDir.getDirName()).absoluteFilePath("test_data1.bin");
    const QByteArray kTestPattern("1234567890");
    const int kPatternRepCnt = 2000 * 499;

    // write file
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
