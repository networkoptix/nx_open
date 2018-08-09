#include <gtest/gtest.h>

#include <nx/network/mac_address.h>

namespace nx::network::test {

class MacAddressTest: public ::testing::Test
{
public:
    static void assertMacAddressIsValid(const QString& address)
    {
        QnMacAddress mac(address);
        ASSERT_FALSE(mac.isNull());
    }

    static void assertMacAddressIsInvalid(const QString& address)
    {
        QnMacAddress mac(address);
        ASSERT_TRUE(mac.isNull());
    }
};

TEST_F(MacAddressTest, parseRawString)
{
    assertMacAddressIsValid("00:11:22:33:44:55");
    assertMacAddressIsValid("00-11-22-33-44-55");
    assertMacAddressIsValid("001122334455");

    assertMacAddressIsInvalid("00:11:22:33:44:55:");
    assertMacAddressIsInvalid("00-11-22-33-44-XX");
    assertMacAddressIsInvalid("0X1122334455");
    assertMacAddressIsInvalid("0011223344556");
}

TEST_F(MacAddressTest, parseBytes)
{
    unsigned char rawData[6];
    for (int i = 0; i < 6; ++i)
        rawData[i] = i;
    auto mac = QnMacAddress::fromRawData(rawData);
    ASSERT_FALSE(mac.isNull());
    ASSERT_EQ(mac.toString().toStdString(), "00-01-02-03-04-05");
}

TEST_F(MacAddressTest, checkToString)
{
    QnMacAddress mac(QString("00:11:22:33:44:55"));
    ASSERT_EQ(mac.toString().toStdString(), "00-11-22-33-44-55");
}

} // namespace nx::network::test
