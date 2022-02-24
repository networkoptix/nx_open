// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <memory>

#include <licensing/license.h>
#include <nx/vms/api/data/license_data.h>
#include <nx/vms/client/desktop/licensing/customer_support.h>
#include <nx/vms/common/html/html.h>

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

//-------------------------------------------------------------------------------------------------
// Validate type deduction and input parsing.

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
    return {address, Channel::email, common::html::mailtoLink(address)};
}

ContactAddressTestCase expectLink(QString address)
{
    return {address, Channel::link, common::html::link(nx::utils::Url::fromUserInput(address))};
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

class ContactAddressConstructorTest: public ::testing::TestWithParam<ContactAddressTestCase>
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

INSTANTIATE_TEST_SUITE_P(ContactAddressParserTest,
    ContactAddressConstructorTest,
    ::testing::ValuesIn(kContactAddressTestCases));

//-------------------------------------------------------------------------------------------------
// Validate regional support calculations.
namespace regional_support {

QnLicensePtr makeLicense(const QString& name, const QString& support)
{
    nx::vms::api::DetailedLicenseData data;
    data.name = name;
    data.company = support;
    data.support = support;
    return QnLicensePtr(new QnLicense(data));
}

/**
 * Check different scenarios with licenses regional support. For test calculations we will use
 * set of licenses:
 *  * A - with regional support a
 *  * B - with regional support b
 *  * X - without regional support
 */
enum class License { a, b, x };
enum class Support { a, b, unknown };

static const QMap<Support, QString> kSupport{
    {Support::a, "a"},
    {Support::b, "b"},
    {Support::unknown, QString()},
};

static const QMap<License, QnLicensePtr> kLicenses{
    {License::a, makeLicense("A", kSupport[Support::a])},
    {License::b, makeLicense("B", kSupport[Support::b])},
    {License::x, makeLicense("X", QString())},
};

static const QnLicenseList kTestLicensesSet{
    kLicenses[License::a],
    kLicenses[License::b],
    kLicenses[License::x]
};

struct RegionalSupportTestCase
{
    std::vector<License> givenLicenses;
    std::vector<Support> expectedRegionalSupport;
};

class RegionalSupportTest: public ::testing::TestWithParam<RegionalSupportTestCase>
{
protected:
    const CustomerSupport m_customerSupport{/*supportContact*/ QString(), kTestLicensesSet};
};

TEST_P(RegionalSupportTest, regionalContactsForLicenses)
{
    const RegionalSupportTestCase& testCase = GetParam();

    QnLicenseList actualLicenses;
    for (auto license: testCase.givenLicenses)
        actualLicenses.push_back(kLicenses[license]);

    std::vector<CustomerSupport::Contact> contacts =
        m_customerSupport.regionalContactsForLicenses(actualLicenses);

    std::vector<Support> result;
    for (auto contact: contacts)
    {
        Support support = kSupport.key(contact.company, Support::unknown);
        ASSERT_NE(support, Support::unknown);
        result.push_back(support);
    }
    ASSERT_EQ(result, testCase.expectedRegionalSupport);
}

std::vector<Support> kAllContacts{
    Support::a,
    Support::b};

static const std::vector<RegionalSupportTestCase> kRegionalSupportTestCases = {
    { {License::a}, {Support::a} },
    { {License::b}, {Support::b} },
    { {License::x}, kAllContacts },
    { {License::a, License::b}, kAllContacts },
    { {License::a, License::x}, kAllContacts },
    { {License::b, License::x}, kAllContacts },
};

INSTANTIATE_TEST_SUITE_P(RegionalSupportContactsTest,
    RegionalSupportTest,
    ::testing::ValuesIn(kRegionalSupportTestCases));

} // namespace regional_support

} // namespace test
} // namespace nx::vms::client::desktop
