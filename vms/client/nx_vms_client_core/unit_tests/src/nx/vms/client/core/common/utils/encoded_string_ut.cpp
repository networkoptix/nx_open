#include <gtest/gtest.h>

#include <nx/vms/client/core/common/utils/encoded_string.h>

namespace nx::vms::client::core {
namespace test {

namespace {

static const QString kSample("password");
static const QString kEncodedWithDefaultKey("2ef7063ccb5885cbbfd89c40d1a42d29");
static const QByteArray kCustomKey("0123456789012345");
static const QString kEncodedWithCustomKey("ff3b1fd59d619cb9e3623c0d74375210");

} // namespace

TEST(EncodedString, emptyStringIsEmpty)
{
    EncodedString empty;
    ASSERT_EQ(empty.decoded(), QString());
    ASSERT_EQ(empty.encoded(), QString());
    ASSERT_TRUE(empty.isEmpty());
}

TEST(EncodedString, encodedStringFromDecodedDiffers)
{
    const auto s = EncodedString::fromDecoded(kSample);
    ASSERT_EQ(s.decoded(), kSample);
    ASSERT_EQ(s.encoded(), kEncodedWithDefaultKey);
    ASSERT_NE(s.decoded(), s.encoded());
    ASSERT_FALSE(s.isEmpty());
}

TEST(EncodedString, decodeFromEncodedWithDefaultKey)
{
    const auto s = EncodedString::fromEncoded(kEncodedWithDefaultKey);
    ASSERT_EQ(s.encoded(), kEncodedWithDefaultKey);
    ASSERT_EQ(s.decoded(), kSample);
    ASSERT_NE(s.decoded(), s.encoded());
    ASSERT_FALSE(s.isEmpty());
}

TEST(EncodedString, encodedWithCustomKeyStringDiffers)
{
    const auto s = EncodedString::fromDecoded(kSample, kCustomKey);
    ASSERT_EQ(s.decoded(), kSample);
    ASSERT_EQ(s.encoded(), kEncodedWithCustomKey);
    ASSERT_NE(s.decoded(), s.encoded());
    ASSERT_FALSE(s.isEmpty());
}

TEST(EncodedString, decodeFromEncodedWithCustomKey)
{
    const auto s = EncodedString::fromEncoded(kEncodedWithCustomKey, kCustomKey);
    ASSERT_EQ(s.decoded(), kSample);
    ASSERT_EQ(s.encoded(), kEncodedWithCustomKey);
    ASSERT_NE(s.decoded(), s.encoded());
    ASSERT_FALSE(s.isEmpty());
}

TEST(EncodedString, initializeDecodedAfterConstructing)
{
    EncodedString s;
    s.setDecoded(kSample);
    ASSERT_EQ(s.encoded(), kEncodedWithDefaultKey);
}

TEST(EncodedString, initializeDecodedAfterConstructingWithCustomKey)
{
    EncodedString s(kCustomKey);
    s.setDecoded(kSample);
    ASSERT_EQ(s.encoded(), kEncodedWithCustomKey);
}

TEST(EncodedString, changeDecodedKey)
{
    auto s = EncodedString::fromDecoded(kSample);
    s.setKey(kCustomKey);
    ASSERT_EQ(s.encoded(), kEncodedWithCustomKey);
}

TEST(EncodedString, initializeEncodedAfterConstructing)
{
    EncodedString s;
    s.setEncoded(kEncodedWithDefaultKey);
    ASSERT_EQ(s.decoded(), kSample);
}

TEST(EncodedString, initializeEncodedAfterConstructingWithCustomKey)
{
    EncodedString s(kCustomKey);
    s.setEncoded(kEncodedWithCustomKey);
    ASSERT_EQ(s.decoded(), kSample);
}

TEST(EncodedString, changeEncodedKey)
{
    auto s = EncodedString::fromEncoded(kEncodedWithCustomKey);
    s.setKey(kCustomKey);
    ASSERT_EQ(s.decoded(), kSample);
}

} // namespace test
} // namespace nx::vms::client::core
