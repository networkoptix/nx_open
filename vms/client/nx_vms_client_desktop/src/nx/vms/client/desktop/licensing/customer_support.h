#pragma once

#include <vector>

#include <QtCore/QString>

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
    CustomerSupport(QnCommonModule* commonModule);

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
    };

    /** Local support of the current system or central application support. */
    const Contact systemContact;

    /** Contact for manual licenses activation. */
    const Contact licensingContact;

    /** Regional application support. */
    std::vector<Contact> regionalContacts;
};

} // namespace nx::vms::client::desktop
