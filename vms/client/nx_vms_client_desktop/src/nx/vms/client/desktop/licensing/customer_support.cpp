// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "customer_support.h"

#include <set>

#include <licensing/license.h>
#include <nx/branding.h>
#include <nx/utils/log/assert.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/common/html/html.h>
#include <nx/vms/common/system_settings.h>
#include <utils/email/email.h>

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

    // Link with the predefined text.
    if (QUrl::fromUserInput(address).isValid())
        return ContactAddress::Channel::link;

    return ContactAddress::Channel::plainText;
}

QString makeHref(ContactAddress::Channel channel, const QString& address)
{
    using namespace nx::vms::common;

    switch (channel)
    {
        case ContactAddress::Channel::empty:
            return QString();
        case ContactAddress::Channel::email:
            return html::mailtoLink(address);
        case ContactAddress::Channel::phone:
        case ContactAddress::Channel::textLink:
        case ContactAddress::Channel::plainText:
            return address;
        case ContactAddress::Channel::link:
            return html::link(nx::utils::Url::fromUserInput(address));
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

CustomerSupport::CustomerSupport(SystemContext* systemContext):
    CustomerSupport(
        systemContext->globalSettings()->emailSettings().supportAddress,
        systemContext->licensePool()->getLicenses())
{
}

CustomerSupport::CustomerSupport(
    const QString& supportContact,
    const QnLicenseList& systemLicenses)
    :
    systemContact{nx::branding::company(), supportContact},
    licensingContact{nx::branding::company(), nx::branding::licensingAddress()},
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

QString CustomerSupport::Contact::toString() const
{
    QString result = company;
    if (!address.href.isEmpty())
        result +=  QString(": ") + address.href;
    return result;
}

} // namespace nx::vms::client::desktop
