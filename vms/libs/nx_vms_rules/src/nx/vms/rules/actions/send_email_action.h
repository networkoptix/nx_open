// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/rules/basic_action.h>

#include "../data_macros.h"

namespace nx::vms::rules {

class NX_VMS_RULES_API SendEmailAction: public nx::vms::rules::BasicAction
{
    Q_OBJECT
    Q_CLASSINFO("type", "nx.actions.sendEmail")

    FIELD(UuidSelection, users, setUsers)
    FIELD(int, interval, setInterval)
    FIELD(QString, emails, setEmails)
    FIELD(QString, caption, setCaption)
    FIELD(QString, description, setDescription)

public:
    static const ItemDescriptor& manifest();
};

} // namespace nx::vms::rules
