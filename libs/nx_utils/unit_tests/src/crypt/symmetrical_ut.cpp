// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <vector>
#include <array>
#include <string>

#include <gtest/gtest.h>

#include <QtCore/QString>

#include <nx/utils/crypt/symmetrical.h>

namespace {

const std::array<uint8_t, 16> kArrayKey = {
    0x2b, 0x00, 0x15, 0x00, 0x28, 0xae, 0xd2, 0xa6,
    0xab, 0xf7, 0x15, 0x88, 0x00, 0xcf, 0x4f, 0x3c
};

const std::vector<std::vector<uint8_t>> kVariableKeys = {
    {},
    {0x00},
    {0x10},
    {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
     0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f},
    {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
     0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x76, 0xff},
    {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
     0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x76, 0xff,
     0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x76, 0xff},
};

/**
 * The escaped unicode codepoints represent cyrillic letters, but they don't form coherent text,
 * it's only test data.
 */
const std::vector<QByteArray> kTestData{
    "1",
    "",
    "abcdefghefjklmnoprstuvwxyz",
    "abcdefghefjklmnopf\u0430\u0432\u044b\u0444 \u0444\u044b\u0432\u0430\u0432 rstuvwxyzabcdefg "
        "hefj klmnoprstuvwxyzabcdefghefjklmnoprstuvwxyz",
    "0",
    "00",
    "0123456789",
    "9876543 210",
    "101",
    "0bcdefg hefjklmnoprstuvwxyzabcdefgh efj9lmno3rs0uv1xyzabcdefghefjklmnoprstuvwxy0 \
     0bcdefghefjklmnoprstuvwxyzabcdefghefj9lmno3rs0uv1xyzabcdefghefjklmnoprstuvwxy0",
    "0bcdefghefjklmnoprstuvwxyzab\u0430\u0432\u044b\u0444 \u0444\u044b\u0432\u0430\u0444\u044b  "
        "\u0432\u0444\u044b\u0430\u0432\u044b\u0444\u0430 \u0444\u044b\u0432\u0430cdefghe fj9lmn"
        "o3rs0uv1xyzabcdefghefjklmnoprstuvwxy0",
    "\u0430\u0432\u044b\u0444\u043b\u0434\u0436 \u0432\u044b\u0444\u0430\u0434\u0436\u043b\u0443"
        "\u043b\u0434\u043e\u0449\u0448 \u043b\u0434903\u043b\u04340 9\u0448\u044912",
    "\u0430\u0432\u044b\u0444\u043b\u0434\u0436 \u0432\u044b\u0444\u0430\u0434\u0436\u043b\u0443"
        "\u043b\u0434\u043e\u0449\u0448 \u043b\u0434903\u043b\u04340 9\u0448\u044912 \u0430\u044b"
        "\u0444\u0434\u043b\u0430\u0444\u044b\u0434\u0436\u0432\u043b\u0430\u0432\u044b\u0434"
        "\u0436\u043b\u0430\u043e\u0434\u0436\u043b\u0430\u0443\u0446\u0439\u0430\u0449\u0448"
        "\u0437\u0443\u0446\u043e\u0430\u0449\u0448 \u043e\u044b\u0432\u0430\u043b \u0434\u0443"
        "\u0446\u0439\u0449\u0448\u0437\u0430\u043a \u0443\u0446\u0449\u0439\u0437\u043e",
};

} // namespace

TEST(SymmetricalEncryptionCBC, AesExtraKey)
{
    const auto key = nx::crypt::generateAesExtraKey();
    ASSERT_EQ(key.size(), 16);
}

TEST(SymmetricalEncryptionCBC, ArrayKey)
{
    for (const auto& data : kTestData)
    {
        auto encryptedData = nx::crypt::encodeAES128CBC(data, kArrayKey);
        ASSERT_EQ(nx::crypt::decodeAES128CBC(encryptedData, kArrayKey), data);
    }
}

TEST(SymmetricalEncryptionCBC, ByteArrayVariableKeys)
{
    for (const auto& k : kVariableKeys)
    {
        for (const auto& data : kTestData)
        {
            QByteArray baKey((const char*)k.data(), k.size());
            auto encryptedData = nx::crypt::encodeAES128CBC(data, baKey);
            ASSERT_EQ(nx::crypt::decodeAES128CBC(encryptedData, baKey), data);
        }
    }
}

TEST(SymmetricalEncryptionCBC, BuiltInKey)
{
    for (const auto& data : kTestData)
    {
        auto encryptedData = nx::crypt::encodeAES128CBC(data);
        ASSERT_EQ(nx::crypt::decodeAES128CBC(encryptedData), data);
    }
}

TEST(SymmetricalEncryptionCBC, StringFunctionsBuiltInKey)
{
    for (const auto& data : kTestData)
    {
        auto dataString = QString::fromUtf8(data);
        auto encryptedString = nx::crypt::encodeHexStringFromStringAES128CBC(dataString);
        ASSERT_EQ(nx::crypt::decodeStringFromHexStringAES128CBC(encryptedString), dataString);
    }
}

TEST(SymmetricalEncryptionCBC, StringFunctionsBAKeys)
{
    for (const auto& k : kVariableKeys)
    {
        for (const auto& data : kTestData)
        {
            auto dataString = QString::fromUtf8(data);
            QByteArray baKey((const char*)k.data(), k.size());
            auto encryptedString = nx::crypt::encodeHexStringFromStringAES128CBC(dataString, baKey);
            ASSERT_EQ(nx::crypt::decodeStringFromHexStringAES128CBC(encryptedString, baKey), dataString);
        }
    }
}
