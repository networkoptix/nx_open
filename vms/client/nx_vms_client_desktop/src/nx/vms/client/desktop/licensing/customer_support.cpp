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

} // namespace

ContactAddress::ContactAddress(const QString& address):
    channel(detectChannel(address)),
    href(makeHref(channel, address)),
    rawData(address)
{
}

CustomerSupport::CustomerSupport(QnCommonModule* commonModule):
    systemContact{
        nx::utils::AppInfo::organizationName(),
        commonModule->globalSettings()->emailSettings().supportEmail},
    licensingContact{
        nx::utils::AppInfo::organizationName(),
        nx::utils::AppInfo::licensingAddress()}
{
    std::set<QnLicense::RegionalSupport> licenseRegionalContacts;

    for (const QnLicensePtr& license: commonModule->licensePool()->getLicenses())
    {
        if (license->type() == Qn::LC_Trial)
            continue;

        const QnLicense::RegionalSupport regionalSupport = license->regionalSupport();
        if (regionalSupport.isValid())
            licenseRegionalContacts.insert(regionalSupport);
    }

    for (const auto& contact: licenseRegionalContacts)
        regionalContacts.push_back({contact.company, contact.address});
}

CustomerSupport::Contact::Contact(const QString& company, const QString& address):
    company(company),
    address(address)
{
}

} // namespace nx::vms::client::desktop
