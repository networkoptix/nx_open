// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>
#include <array>

#include <nx/utils/byte_stream/custom_output_stream.h>
#include <nx/utils/gzip/gzip_compressor.h>
#include <nx/utils/gzip/gzip_uncompressor.h>

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

TEST(Gzip, UncompressorRejectsDecompressionBomb)
{
    // A highly compressible payload that inflates far beyond the cap.
    const nx::Buffer origin(1 * 1024 * 1024, 'A'); //< 1 MiB of 'A' compresses to ~1 KiB.
    const nx::Buffer compressed = Compressor::compressData(origin, /*addCrcAndSize*/ false);
    ASSERT_LT(compressed.size(), origin.size() / 100);

    nx::Buffer decompressed;
    auto sink = nx::utils::bstream::makeCustomOutputStream(
        [&decompressed](const nx::ConstBufferRefType& data) { decompressed.append(data); });

    Uncompressor uncompressor(sink);
    static constexpr std::uint64_t kCap = 64 * 1024;
    uncompressor.setMaxOutputSize(kCap);

    ASSERT_FALSE(uncompressor.processData(compressed));
    ASSERT_LE(decompressed.size(), kCap);

    // Subsequent calls must keep failing even with new data and flush.
    ASSERT_FALSE(uncompressor.processData(compressed));
    ASSERT_EQ(0u, uncompressor.flush());
}

TEST(Gzip, UncompressorAllowsDataBelowLimit)
{
    const nx::Buffer origin = "hello, world";
    const nx::Buffer compressed = Compressor::compressData(origin, /*addCrcAndSize*/ false);

    nx::Buffer decompressed;
    auto sink = nx::utils::bstream::makeCustomOutputStream(
        [&decompressed](const nx::ConstBufferRefType& data) { decompressed.append(data); });

    Uncompressor uncompressor(sink);
    uncompressor.setMaxOutputSize(1024);

    ASSERT_TRUE(uncompressor.processData(compressed));
    ASSERT_EQ(decompressed, origin);
}

} // namespace nx::utils::test
