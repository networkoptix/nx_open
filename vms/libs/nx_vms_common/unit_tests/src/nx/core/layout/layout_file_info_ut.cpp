// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/core/layout/layout_file_info.h>
#include <nx/core/layout/layout_file_info_v1.h>
#include <nx/utils/crypt/crypted_file_stream.h>
#include <nx/utils/test_support/test_options.h>

using namespace nx::core::layout;
using nx::TestOptions;

static const char* dummyName = "DeleteMe.exe";
static const char* thePassword = "helloworld";
static constexpr int kCountOfIntegers = 100;

static void writeTestFileVersion1(const QString& name)
{
    QFile file(name);
    ASSERT_TRUE(file.open(QFile::WriteOnly));

    uint64_t x = 0x1eaddeaddeaddead;

    for (int i = 0; i < kCountOfIntegers; i++)
        file.write((char *) &x, sizeof(quint64));

    StreamIndexV1 index;
    index.magic = StreamIndexV1::kIndexCryptedMagic;
    file.write((char *) &index, sizeof(StreamIndexV1));

    CryptoInfo crypto;
    crypto.passwordSalt = nx::crypt::getRandomSalt();
    crypto.passwordHash =
        nx::crypt::getSaltedPasswordHash(thePassword, crypto.passwordSalt);
    file.write((char *) &crypto, sizeof(CryptoInfo));

    for (int i = 0; i < kCountOfIntegers; i++)
        file.write((char *) &x, sizeof(quint64));

    quint64 offset = kCountOfIntegers * sizeof(quint64);
    quint64 magic = StreamIndexV1::kFileMagic;

    file.write((char *) &offset, sizeof(quint64));
    file.write((char *) &magic, sizeof(quint64));
}

static void writeTestFileVersion2(const QString& name)
{
    QFile file(name);
    ASSERT_TRUE(file.open(QFile::WriteOnly));

    uint64_t x = 0x1eaddeaddeaddead;

    for (int i = 0; i < kCountOfIntegers; i++)
        file.write((char *) &x, sizeof(quint64));

    Footer footer;
    footer.offset = file.size();

    Header header;
    header.hasCryptoData = 0;
    header.entryCount = 2;
    file.write((char *)&header, sizeof(header));

    StreamIndexEntry entry;
    entry.offset = 1;
    file.write((const char*)&entry, sizeof(entry));
    entry.offset = 234;
    file.write((const char*)&entry, sizeof(entry));
    file.write((char *)&footer, sizeof(footer));
}

TEST(LayoutFileInfo, BasicParsing)
{
    const auto fileName = TestOptions::temporaryDirectoryPath(true) + dummyName;

    writeTestFileVersion2(fileName);

    auto fileInfo = readNovFileIndex(fileName);

    ASSERT_TRUE(fileInfo);
    ASSERT_TRUE(!fileInfo->cryptoInfo);
    ASSERT_EQ(fileInfo->entries.size(), 2);
    ASSERT_EQ(fileInfo->entries[0].offset, 1);
    ASSERT_EQ(fileInfo->entries[1].offset, 234);
    QFile::remove(fileName);
}

TEST(LayoutFileInfo, BackwardCompatibility)
{
    const auto fileName = TestOptions::temporaryDirectoryPath(true) + dummyName;

    writeTestFileVersion1(fileName);

    auto fileInfo = readNovFileIndex(fileName);

    ASSERT_TRUE(fileInfo);
    ASSERT_TRUE(fileInfo->cryptoInfo);
    QFile::remove(fileName);
}

TEST(LayoutFileInfo, TestIndexSize)
{
    FileInfo info;
    info.cryptoInfo = CryptoInfo();
    info.entries.push_back(StreamIndexEntry{});
    info.entries.push_back(StreamIndexEntry{});

    // 8        /sizeof(Header)/
    // + 256    /sizeof(CryptoInfo)/
    // + 16 * 2 /sizeof(StreamIndexEntry) * 2/
    // + 20     /sizeof(Footer)/
    // = 316.
    ASSERT_EQ(sizeof(CryptoInfo), 256);
    ASSERT_EQ(info.totalIndexHeaderSize(), 316);
}
