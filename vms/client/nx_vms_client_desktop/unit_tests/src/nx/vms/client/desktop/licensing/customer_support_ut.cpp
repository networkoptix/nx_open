#include <gtest/gtest.h>

#include <nx/vms/client/desktop/licensing/customer_support.h>

#include <utils/common/html.h>

namespace nx::vms::client::desktop {

using Channel = ContactAddress::Channel;

void PrintTo(const Channel& channel, ::std::ostream* os)
{
    std::map<Channel, std::string> stringValues
    {
        {Channel::empty, "empty"},
        {Channel::email, "email"},
        {Channel::link, "link"},
        {Channel::phone, "phone"},
    };

    *os << stringValues[channel];
}

namespace test {

struct ContactAddressTestCase
{
    QString address;
    Channel expectedChannel;
    QString expectedHref;
};

void PrintTo(const ContactAddressTestCase& testCase, ::std::ostream* os)
{
    *os << testCase.address.toStdString();
}

ContactAddressTestCase expectEmail(QString address)
{
    return {address, Channel::email, makeMailHref(address)};
}

ContactAddressTestCase expectLink(QString address)
{
    return {address, Channel::link, makeHref(address)};
}

ContactAddressTestCase expectTextLink(QString address)
{
    return {address, Channel::textLink, address};
}

ContactAddressTestCase expectPhone(QString address)
{
    return {address, Channel::phone, address};
}

static const std::vector<ContactAddressTestCase> kContactAddressTestCases = {
    { "", Channel::empty, "" },
    expectEmail("email+address@mail.com"),
    expectPhone("+1234567890"),
    expectLink("http://address.com"),
    expectLink("address.com"),
    expectTextLink(R"html(<a href="https://address.com">Address</a>)html"),
};

class ContactAddressConstructorTest : public ::testing::TestWithParam<ContactAddressTestCase>
{
};

TEST_P(ContactAddressConstructorTest, constructedCorrectly)
{
    const ContactAddressTestCase& testCase = GetParam();
    const ContactAddress actual(testCase.address);
    ASSERT_EQ(actual.rawData, testCase.address);
    ASSERT_EQ(actual.channel, testCase.expectedChannel);
    ASSERT_EQ(actual.href, testCase.expectedHref);
}

INSTANTIATE_TEST_CASE_P(ContactAddressParserTest,
    ContactAddressConstructorTest,
    ::testing::ValuesIn(kContactAddressTestCases));

} // namespace test
} // namespace nx::vms::client::desktop
