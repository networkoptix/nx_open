#include "customer_support.h"

#include <api/global_settings.h>
#include <common/common_module.h>
#include <licensing/license.h>

#include <utils/email/email.h>
#include <utils/common/html.h>

#include <nx/utils/app_info.h>
#include <nx/utils/log/assert.h>

namespace nx::vms::client::desktop {

namespace {

ContactAddress::Channel detectChannel(const QString& address)
{
    if (address.isEmpty())
        return ContactAddress::Channel::empty;

    // Check if email is provided.
    QnEmailAddress email(address);
    if (email.isValid())
        return ContactAddress::Channel::email;

    // Simple check if phone is provided.
    if (address.startsWith("+"))
        return ContactAddress::Channel::phone;

    // Link, which is preconfigured in the customization.
    if (address.startsWith("<a href="))
        return ContactAddress::Channel::textLink;

    // In all uncertain cases try to make it a link.
    return ContactAddress::Channel::link;
}

QString makeHref(ContactAddress::Channel channel, const QString& address)
{
    switch (channel)
    {
        case ContactAddress::Channel::empty:
            return QString();
        case ContactAddress::Channel::email:
            return makeMailHref(address);
        case ContactAddress::Channel::phone:
        case ContactAddress::Channel::textLink:
            return address;
        case ContactAddress::Channel::link:
            return makeHref(address);
    }
    NX_ASSERT(false, "Unhandled switch case");
    return address;
}

std::vector<CustomerSupport::Contact> getRegionalContactsForLicenses(
    const QnLicenseList& licenses,
    bool* hasLicensesWithoutSupport = nullptr)
{
    if (hasLicensesWithoutSupport)
        *hasLicensesWithoutSupport = false;

    std::set<QnLicense::RegionalSupport> licenseRegionalContacts;
    for (const QnLicensePtr& license: licenses)
    {
        // Ignore unsupported licenses, e.g. demo licenses, provided by the support team.
        if (license->type() == Qn::LC_Trial)
            continue;

        const QnLicense::RegionalSupport regionalSupport = license->regionalSupport();
        if (regionalSupport.isValid())
            licenseRegionalContacts.insert(regionalSupport);
        else if (hasLicensesWithoutSupport)
            *hasLicensesWithoutSupport = true;
    }

    std::vector<CustomerSupport::Contact> result;
    for (const auto& contact: licenseRegionalContacts)
        result.push_back({contact.company, contact.address});

    return result;
}

} // namespace

ContactAddress::ContactAddress(const QString& address):
    channel(detectChannel(address)),
    href(makeHref(channel, address)),
    rawData(address)
{
}

CustomerSupport::CustomerSupport(QnCommonModule* commonModule):
    CustomerSupport(
        commonModule->globalSettings()->emailSettings().supportEmail,
        commonModule->licensePool()->getLicenses())
{
}

CustomerSupport::CustomerSupport(
    const QString& supportContact,
    const QnLicenseList& systemLicenses)
    :
    systemContact{nx::utils::AppInfo::organizationName(), supportContact},
    licensingContact{nx::utils::AppInfo::organizationName(), nx::utils::AppInfo::licensingAddress()},
    regionalContacts(getRegionalContactsForLicenses(systemLicenses))
{
}

std::vector<CustomerSupport::Contact> CustomerSupport::regionalContactsForLicenses(
    const QnLicenseList& licenses) const
{
    bool hasLicensesWithoutSupport = false;
    std::vector<CustomerSupport::Contact> result = getRegionalContactsForLicenses(
        licenses,
        &hasLicensesWithoutSupport);

    if (hasLicensesWithoutSupport)
        return regionalContacts;

    return result;
}

CustomerSupport::Contact::Contact(const QString& company, const QString& address):
    company(company),
    address(address)
{
}

} // namespace nx::vms::client::desktop
