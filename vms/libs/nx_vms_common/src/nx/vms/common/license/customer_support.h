// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <vector>

#include <QtCore/QCoreApplication>
#include <QtCore/QString>

#include <licensing/license_fwd.h>

namespace nx::vms::common {

class SystemContext;

/**
 * Can contain an email, a web site url or a phone number.
 */
struct NX_VMS_COMMON_API ContactAddress
{
    ContactAddress(const QString& address);

    enum class Channel
    {
        empty,
        email,
        phone,
        link,
        textLink, //< Link with the predefined text.
        plainText,
    };

    const Channel channel;
    const QString href;
    const QString rawData;
};

struct NX_VMS_COMMON_API CustomerSupport
{
    Q_DECLARE_TR_FUNCTIONS(CustomerSupport)

public:
    /** Get customer support contacts for the whole system. */
    explicit CustomerSupport(SystemContext* systemContext);

    /**
     * Get customer support contacts for the whole system.
     * Explicit constructor for the network-free unit tests, which could not afford common module.
     * @param supportContact System-wide support contact.
     * @param systemLicenses Full list of licenses in the system.
     */
    CustomerSupport(const QString& supportContact, const QnLicenseList& systemLicenses);

    struct NX_VMS_COMMON_API Contact
    {
        /**
         * Supporter company name.
         */
        const QString company;

        /**
         * Contact address. Can contain an email, a web site url or a phone number.
         */
        const ContactAddress address;

        Contact(const QString& company, const QString& address);

        QString toString() const;
    };

    /**
     * Get customer support contacts for the selected licenses. Actual e.g. while deactivating them.
     * If at least one of the licenses have no regional support contacts, system-wide contacts are
     * used instead.
     */
    std::vector<Contact> regionalContactsForLicenses(const QnLicenseList& licenses) const;

    /** Local support of the current system or central application support. */
    const Contact systemContact;

    /** Contact for manual licenses activation. */
    const Contact licensingContact;

    /** Regional application support. */
    const std::vector<Contact> regionalContacts;

    /**
     * Create a support message, referring to regional support, mentioned in the provided licenses.
     * If no licenses are provided, all system licenses are used.
     */
    static QString customerSupportMessage(
        SystemContext* context,
        const QString& genericString,
        const QString& regionalSupportString,
        std::optional<QnLicenseList> limitToLicenses = std::nullopt);
};

} // namespace nx::vms::common
