// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <vector>

#include <QtCore/QString>

#include <licensing/license_fwd.h>

class QnCommonModule;

namespace nx::vms::client::desktop {

/**
 * Can contain an email, a web site url or a phone number.
 */
struct NX_VMS_CLIENT_DESKTOP_API ContactAddress
{
    ContactAddress(const QString& address);

    enum class Channel
    {
        empty,
        email,
        phone,
        link,
        textLink, //< Link with the predefined text.
    };

    const Channel channel;
    const QString href;
    const QString rawData;
};

struct NX_VMS_CLIENT_DESKTOP_API CustomerSupport
{
    /** Get customer support contacts for the whole system. */
    explicit CustomerSupport(QnCommonModule* commonModule);

    /**
     * Get customer support contacts for the whole system.
     * Explicit constructor for the network-free unit tests, which could not afford common module.
     * @param supportContact System-wide support contact.
     * @param systemLicenses Full list of licenses in the system.
     */
    CustomerSupport(const QString& supportContact, const QnLicenseList& systemLicenses);

    struct Contact
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
};

} // namespace nx::vms::client::desktop
