// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "ldap_sync_issue_event.h"

#include "../utils/type.h"

namespace nx::vms::rules {

LdapSyncIssueEvent::LdapSyncIssueEvent(std::chrono::microseconds timestamp):
    BasicEvent(timestamp)
{
}

QString LdapSyncIssueEvent::resourceKey() const
{
    return {};
}

QVariantMap LdapSyncIssueEvent::details(common::SystemContext* context) const
{
    auto result = BasicEvent::details(context);
    return result;
}

const ItemDescriptor& LdapSyncIssueEvent::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = utils::type<LdapSyncIssueEvent>(),
        .displayName = tr("Ldap Sync Issue Event"),
        .description = "",
        .fields = {
        },
        .emailTemplatePath = ":/email_templates/ldap_sync_issue.mustache"
    };
    return kDescriptor;
}

} // namespace nx::vms::rules
