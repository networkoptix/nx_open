// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/rules/basic_event.h>

#include "../data_macros.h"

namespace nx::vms::rules {

class NX_VMS_RULES_API LdapSyncIssueEvent: public BasicEvent
{
    Q_OBJECT
    Q_CLASSINFO("type", "nx.events.ldapSyncIssue")

public:
    LdapSyncIssueEvent() = default;
    LdapSyncIssueEvent(
        std::chrono::microseconds timestamp);

    virtual QString resourceKey() const override;
    virtual QVariantMap details(common::SystemContext* context) const override;

    static const ItemDescriptor& manifest();

};

} // namespace nx::vms::rules
