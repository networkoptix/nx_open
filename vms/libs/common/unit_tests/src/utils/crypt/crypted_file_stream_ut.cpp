#include <gtest/gtest.h>
#include <utils/crypt/crypted_file_stream.h>

#include <nx/utils/test_support/test_options.h>

static const int bufferSize = 9000;
static char buffer[bufferSize];

static const char* dummyName = "DeleteMe.bin";

static const char* thePassword = "helloworld";

using nx::utils::TestOptions;

static void writeTestFile(const QString& name)
{
    nx::utils::CryptedFileStream stream(name, thePassword);
    ASSERT_TRUE(stream.open(QIODevice::WriteOnly));

    for (int i = 0; i < bufferSize; i++)
        buffer[i] = (char) (i % 255);

    ASSERT_EQ(stream.write(buffer, bufferSize), bufferSize);
    stream.close();
}

TEST(CryptedFileStream, Basic)
{
    const auto fileName = TestOptions::temporaryDirectoryPath() + dummyName;

    nx::utils::CryptedFileStream stream2("NonExistent.bin", nullptr);
    ASSERT_FALSE(stream2.open(QIODevice::ReadOnly));

    writeTestFile(fileName);

    nx::utils::CryptedFileStream stream4(fileName, "Bad Password");
    ASSERT_FALSE(stream4.open(QIODevice::ReadOnly));

    nx::utils::CryptedFileStream stream1(fileName, thePassword);
    ASSERT_TRUE(stream1.open(QIODevice::ReadOnly));

    char buffer1[bufferSize];
    ASSERT_EQ(stream1.read(buffer1, bufferSize), bufferSize);

    for (int i = 0; i < bufferSize; i++)
        ASSERT_TRUE(buffer1[i] == buffer[i]);

    stream1.close();
}

TEST(CryptedFileStream, AppendAndReopen)
{
    const auto fileName = TestOptions::temporaryDirectoryPath() + dummyName;

    writeTestFile(fileName);

    // Appending copy once again.
    nx::utils::CryptedFileStream stream(fileName, thePassword);
    ASSERT_TRUE(stream.open(QIODevice::Append | QIODevice::WriteOnly));
    ASSERT_EQ(stream.write(buffer, bufferSize), bufferSize);
    stream.close();

    // Checking what is inside.
    nx::utils::CryptedFileStream stream1(fileName, thePassword);
    ASSERT_TRUE(stream1.open(QIODevice::ReadOnly));

    const int doubleBufferSize = bufferSize * 2;
    char buffer1[doubleBufferSize];
    ASSERT_EQ(stream1.read(buffer1, doubleBufferSize), doubleBufferSize);

    for (int i = 0; i < doubleBufferSize; i++)
        ASSERT_TRUE(buffer1[i] == buffer[i % bufferSize]);

    stream1.close();

    // Reopening this reading stream again.
    ASSERT_TRUE(stream1.open(QIODevice::ReadOnly));

    ASSERT_EQ(stream1.read(buffer1, doubleBufferSize), doubleBufferSize);

    for (int i = 0; i < doubleBufferSize; i++)
        ASSERT_TRUE(buffer1[i] == buffer[i % bufferSize]);
}

TEST(CryptedFileStream, SeekAndPos)
{
    const auto fileName = TestOptions::temporaryDirectoryPath() + dummyName;

    writeTestFile(fileName);

    nx::utils::CryptedFileStream stream1(fileName, thePassword);
    ASSERT_TRUE(stream1.open(QIODevice::ReadOnly));

    const int posBufferSize = 3000;
    char posBuffer[posBufferSize];
    stream1.read(posBuffer, posBufferSize);

    for (int i = 0; i < posBufferSize; i++)
        ASSERT_TRUE(posBuffer[i] == buffer[i % bufferSize]);

    int k = stream1.pos();
    ASSERT_TRUE(stream1.pos() == posBufferSize);

    const int seekPos = 2000;
    char readBuffer[bufferSize];
    stream1.seek(seekPos);
    stream1.read(readBuffer, bufferSize - seekPos);

    for (int i = 0; i < bufferSize - seekPos; i++)
        ASSERT_TRUE(readBuffer[i] == buffer[i + seekPos]);
}

TEST(CryptedFileStream, EmbeddedMode)
{
    const auto fileName = TestOptions::temporaryDirectoryPath() + dummyName;

    QFile File(fileName);
    File.open(QIODevice::WriteOnly);
    for (int i = 0; i < 5; i++)
        File.write(buffer, bufferSize);
    File.close();

    // Write embedded stream.
    nx::utils::CryptedFileStream stream(fileName, thePassword);
    stream.setEnclosure(1000, 0);
    ASSERT_TRUE(stream.open(QIODevice::WriteOnly));
    ASSERT_EQ(stream.write(buffer, bufferSize), bufferSize);
    qint64 grossSize = stream.grossSize();
    stream.close();

    // Read embedded stream.
    nx::utils::CryptedFileStream stream1(fileName, thePassword);
    stream1.setEnclosure(1000, grossSize);
    ASSERT_TRUE(stream1.open(QIODevice::ReadOnly));
    char buffer1[bufferSize];
    ASSERT_EQ(stream1.read(buffer1, bufferSize), bufferSize);

    for (int i = 0; i < bufferSize; i++)
        ASSERT_TRUE(buffer1[i] == buffer[i]);

    stream1.close();
}