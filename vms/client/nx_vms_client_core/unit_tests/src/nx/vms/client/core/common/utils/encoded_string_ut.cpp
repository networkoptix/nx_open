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
    ASSERT_EQ(empty.value(), QString());
    ASSERT_EQ(empty.encoded(), QString());
    ASSERT_TRUE(empty.isEmpty());
}

TEST(EncodedString, encodedStringDiffers)
{
    EncodedString s(kSample);
    ASSERT_EQ(s.value(), kSample);
    ASSERT_EQ(s.encoded(), kEncodedWithDefaultKey);
    ASSERT_NE(s.value(), s.encoded());
    ASSERT_FALSE(s.isEmpty());
}

TEST(EncodedString, decodeFromEncodedWithDefaultKey)
{
    EncodedString s = EncodedString::fromEncoded(kEncodedWithDefaultKey);
    ASSERT_EQ(s.value(), kSample);
    ASSERT_EQ(s.encoded(), kEncodedWithDefaultKey);
    ASSERT_NE(s.value(), s.encoded());
    ASSERT_FALSE(s.isEmpty());
}

TEST(EncodedString, encodedWithCustomKeyStringDiffers)
{
    EncodedString s(kSample, kCustomKey);
    ASSERT_EQ(s.value(), kSample);
    ASSERT_EQ(s.encoded(), kEncodedWithCustomKey);
    ASSERT_NE(s.value(), s.encoded());
    ASSERT_FALSE(s.isEmpty());
}

TEST(EncodedString, decodeFromEncodedWithCustomKey)
{
    EncodedString s = EncodedString::fromEncoded(kEncodedWithCustomKey, kCustomKey);
    ASSERT_EQ(s.value(), kSample);
    ASSERT_EQ(s.encoded(), kEncodedWithCustomKey);
    ASSERT_NE(s.value(), s.encoded());
    ASSERT_FALSE(s.isEmpty());
}

} // namespace test
} // namespace nx::vms::client::core
