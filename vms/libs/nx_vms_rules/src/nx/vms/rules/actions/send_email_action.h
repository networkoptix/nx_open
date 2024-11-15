// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QVariant>

#include <utils/email/message.h>

#include "../basic_action.h"
#include "../data_macros.h"

namespace nx::vms::rules {

class NX_VMS_RULES_API SendEmailAction: public nx::vms::rules::BasicAction
{
    Q_OBJECT
    Q_CLASSINFO("type", "sendEmail")

    FIELD(nx::vms::rules::UuidSelection, users, setUsers)
    FIELD(std::chrono::microseconds, interval, setInterval)
    FIELD(QString, emails, setEmails)
    FIELD(nx::email::Message, message, setMessage)

public:
    static const ItemDescriptor& manifest();

    QSet<QString> emailAddresses(common::SystemContext* context, bool activeOnly) const;
    virtual QVariantMap details(common::SystemContext* context) const override;
};

} // namespace nx::vms::rules
