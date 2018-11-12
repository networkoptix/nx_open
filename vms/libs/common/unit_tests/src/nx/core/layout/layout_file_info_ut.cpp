#include <gtest/gtest.h>

#include <nx/core/layout/layout_file_info.h>
#include <utils/crypt/crypted_file_stream.h>

#include <nx/utils/test_support/test_options.h>

using namespace nx::core::layout;
using nx::utils::TestOptions;

static const char* dummyName = "DeleteMe.exe";

static const char* thePassword = "helloworld";

static void writeTestFile(const QString& name)
{
    QFile file(name);
    ASSERT_TRUE(file.open(QFile::WriteOnly));

    quint64 x = 0x1eaddeaddeaddead;

    for (int i = 0; i < 10000; i++)
        file.write((char *) &x, sizeof(quint64));

    StreamIndex index;
    index.magic = kIndexCryptedMagic;
    file.write((char *) &index, sizeof(StreamIndex));

    CryptoInfo crypto;
    crypto.passwordSalt = nx::utils::crypto_functions::getRandomSalt();
    crypto.passwordHash =
        nx::utils::crypto_functions::getSaltedPasswordHash(thePassword, crypto.passwordSalt);
    file.write((char *) &crypto, sizeof(CryptoInfo));

    for (int i = 0; i < 10000; i++)
        file.write((char *) &x, sizeof(quint64));

    quint64 offset = 10000 * sizeof(quint64);
    quint64 magic = kFileMagic;

    file.write((char *) &offset, sizeof(quint64));
    file.write((char *) &magic, sizeof(quint64));
}

TEST(LayoutFileInfo, Basic)
{
    const auto fileName = TestOptions::temporaryDirectoryPath() + dummyName;

    writeTestFile(fileName);

    FileInfo fileInfo = identifyFile(fileName);

    ASSERT_TRUE(fileInfo.isValid);
    ASSERT_TRUE(fileInfo.isCrypted);
    ASSERT_TRUE(fileInfo.offset == 10000 * sizeof(quint64));
}

