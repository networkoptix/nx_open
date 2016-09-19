#include <vector>
#include <array>
#include <string>
#include <gtest/gtest.h>
#include <utils/crypt/symmetrical.h>

TEST(SymmetricalEncryption, EncryptDecrypt)
{
    std::array<uint8_t, 16> key = { 
        0x2b, 0x00, 0x15, 0x00, 0x28, 0xae, 0xd2, 0xa6, 
        0xab, 0xf7, 0x15, 0x88, 0x00, 0xcf, 0x4f, 0x3c 
    };
    std::vector<QByteArray> testData = {
        "1",
        "",
        "abcdefghefjklmnoprstuvwxyz",
        "abcdefghefjklmnoprstuvwxyzabcdefghefjklmnoprstuvwxyzabcdefghefjklmnoprstuvwxyz",
        "0",
        "00",
        "0123456789",
        "9876543210",
        "101",
        "0bcdefghefjklmnoprstuvwxyzabcdefghefj9lmno3rs0uv1xyzabcdefghefjklmnoprstuvwxy0 \
         0bcdefghefjklmnoprstuvwxyzabcdefghefj9lmno3rs0uv1xyzabcdefghefjklmnoprstuvwxy0",
        "0bcdefghefjklmnoprstuvwxyzabcdefghefj9lmno3rs0uv1xyzabcdefghefjklmnoprstuvwxy0", 
    };
    for (const auto& data : testData)
    {
        auto encryptedData = nx::utils::encodeAES128CBC(data, key);
        ASSERT_EQ(nx::utils::decodeAES128CBC(encryptedData, key), data);
    }
    
    std::vector<std::vector<uint8_t>> keys = {
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

    for (const auto& k : keys)
    {
        for (const auto& data : testData)
        {
            QByteArray baKey((const char*)k.data(), k.size());
            auto encryptedData = nx::utils::encodeAES128CBC(data, baKey);
            ASSERT_EQ(nx::utils::decodeAES128CBC(encryptedData, baKey), data);
        }
    }
}
