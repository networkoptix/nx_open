#include <gtest/gtest.h>
#include <array>

#include <nx/utils/gzip/gzip_compressor.h>

namespace nx::utils::test {

using namespace nx::utils::bstream::gzip;

template<typename T>
static bool isGzipCompressed(const T& compressed)
{
    return compressed.size() > 10 && '\x1f' == compressed[0] && '\x8b' == compressed[1];
}

template<typename T>
static bool isZipCompressed(const T& compressed)
{
    return compressed.size() > 6 && '\x78' == compressed[0] && '\x9c' == compressed[1];
}

TEST(Gzip, DecompressGzip)
{
    const QByteArray origin = "test";
    const QByteArray compressed = Compressor::compressData(origin, false);
    ASSERT_TRUE(isGzipCompressed(compressed));
    ASSERT_EQ(Compressor::uncompressData(compressed), origin);
}

TEST(Gzip, DecompressZip)
{
    const QByteArray origin = "test";
    const std::array<char, 12> compressed = {
        0x78, '\x9c', 0x2b, 0x49, 0x2d, 0x2e, 0x01, 0x00, 0x04, 0x5d, 0x01, '\xc1'};
    ASSERT_TRUE(isZipCompressed(compressed));
    ASSERT_EQ(
        Compressor::uncompressData(QByteArray(compressed.data(), compressed.size())), origin);
}

TEST(Gzip, DecompressRaw)
{
    const QByteArray origin = "test";
    const std::array<char, 5> compressed = {0x2b, 0x49, 0x2d, 0x2e, 0x01};
    ASSERT_FALSE(isGzipCompressed(compressed));
    ASSERT_FALSE(isZipCompressed(compressed));
    ASSERT_EQ(
        Compressor::uncompressData(QByteArray(compressed.data(), compressed.size())), origin);
}

} // namespace nx::utils::test
