#include <vector>
#include <array>
#include <string>
#include <gtest/gtest.h>
#include <utils/crypt/symmetrical.h>

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

const std::vector<QByteArray> kTestData = {
    u8"1",
    u8"",
    u8"abcdefghefjklmnoprstuvwxyz",
    u8"abcdefghefjklmnopfавыф фывав rstuvwxyzabcdefg hefj klmnoprstuvwxyzabcdefghefjklmnoprstuvwxyz",
    u8"0",
    u8"00",
    u8"0123456789",
    u8"9876543 210",
    u8"101",
    u8"0bcdefg hefjklmnoprstuvwxyzabcdefgh efj9lmno3rs0uv1xyzabcdefghefjklmnoprstuvwxy0 \
     0bcdefghefjklmnoprstuvwxyzabcdefghefj9lmno3rs0uv1xyzabcdefghefjklmnoprstuvwxy0",
    u8"0bcdefghefjklmnoprstuvwxyzabавыф фывафы  вфыавыфа фываcdefghe fj9lmno3rs0uv1xyzabcdefghefjklmnoprstuvwxy0",
    u8"авыфлдж выфаджлулдощш лд903лд0 9шщ12",
    u8"авыфлдж выфаджлулдощш лд903лд0 9шщ12 аыфдлафыджвлавыджлаоджлауцйащшзуцоащш оывал дуцйщшзак уцщйзо",
};
}

TEST(SymmetricalEncryptionCBC, AesExtraKey)
{
    const auto key = nx::utils::generateAesExtraKey();
    ASSERT_EQ(key.size(), 16);
}

TEST(SymmetricalEncryptionCBC, ArrayKey)
{
    for (const auto& data : kTestData)
    {
        auto encryptedData = nx::utils::encodeAES128CBC(data, kArrayKey);
        ASSERT_EQ(nx::utils::decodeAES128CBC(encryptedData, kArrayKey), data);
    }
}

TEST(SymmetricalEncryptionCBC, ByteArrayVariableKeys)
{
    for (const auto& k : kVariableKeys)
    {
        for (const auto& data : kTestData)
        {
            QByteArray baKey((const char*)k.data(), k.size());
            auto encryptedData = nx::utils::encodeAES128CBC(data, baKey);
            ASSERT_EQ(nx::utils::decodeAES128CBC(encryptedData, baKey), data);
        }
    }
}

TEST(SymmetricalEncryptionCBC, BuiltInKey)
{
    for (const auto& data : kTestData)
    {
        auto encryptedData = nx::utils::encodeAES128CBC(data);
        ASSERT_EQ(nx::utils::decodeAES128CBC(encryptedData), data);
    }
}

TEST(SymmetricalEncryptionCBC, StringFunctionsBuiltInKey)
{
    for (const auto& data : kTestData)
    {
        auto dataString = QString::fromUtf8(data);
        auto encryptedString = nx::utils::encodeHexStringFromStringAES128CBC(dataString);
        ASSERT_EQ(nx::utils::decodeStringFromHexStringAES128CBC(encryptedString), dataString);
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
            auto encryptedString = nx::utils::encodeHexStringFromStringAES128CBC(dataString, baKey);
            ASSERT_EQ(nx::utils::decodeStringFromHexStringAES128CBC(encryptedString, baKey), dataString);
        }
    }
}
