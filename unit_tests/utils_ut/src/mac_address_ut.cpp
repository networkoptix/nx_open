#include <gtest/gtest.h>

#include <nx/utils/mac_address.h>

namespace nx::utils::test {

class MacAddressTest: public ::testing::Test
{
public:
    static void assertMacAddressIsValid(const QString& address)
    {
        MacAddress mac(address);
        ASSERT_FALSE(mac.isNull());
    }

    static void assertMacAddressIsInvalid(const QString& address)
    {
        MacAddress mac(address);
        ASSERT_TRUE(mac.isNull());
    }
};

TEST_F(MacAddressTest, parseValidStrings)
{
    assertMacAddressIsValid("00:11:22:33:44:55");
    assertMacAddressIsValid("00-11-22-33-44-55");
    assertMacAddressIsValid("001122334455");
}

TEST_F(MacAddressTest, parseInvalidStrings)
{
    assertMacAddressIsInvalid("00:11:22:33:44:55:");
    assertMacAddressIsInvalid("00-11-22-33-44-XX");
    assertMacAddressIsInvalid("0X1122334455");
    assertMacAddressIsInvalid("0011223344556");
}

TEST_F(MacAddressTest, checkDelimitersPlacement)
{
    // Correct length, correct number of delimiters, each segment is parseable (like '-1').
    assertMacAddressIsInvalid("000-10-20-30-40-5");

    // Correct length, invalid number of delimiters, each segment is parseable (like '-1').
    assertMacAddressIsInvalid("00--1-20-30-40-50");
}

TEST_F(MacAddressTest, checkNegativeSegments)
{
    // Negative but parseable segments (like '-1').
    assertMacAddressIsInvalid("00:-1:-2:-3:-4:-5");
}

TEST_F(MacAddressTest, checkInvalidNotation)
{
    // Parseable but non-standard segments (like '+1').
    assertMacAddressIsInvalid("+0+1+2334455");
    assertMacAddressIsInvalid("0.1.2.334455");
}

TEST_F(MacAddressTest, parseBytes)
{
    unsigned char rawData[6];
    for (int i = 0; i < 6; ++i)
        rawData[i] = i;
    auto mac = MacAddress::fromRawData(rawData);
    ASSERT_FALSE(mac.isNull());
    ASSERT_EQ(mac.toString().toStdString(), "00-01-02-03-04-05");
}

TEST_F(MacAddressTest, checkToString)
{
    MacAddress mac(QString("00:11:22:33:44:55"));
    ASSERT_EQ(mac.toString().toStdString(), "00-11-22-33-44-55");
}

} // namespace nx::utils::test
