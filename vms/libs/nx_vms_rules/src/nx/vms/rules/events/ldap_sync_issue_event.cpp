// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "ldap_sync_issue_event.h"

#include <chrono>

#include <nx/vms/rules/aggregated_event.h>
#include <nx/vms/rules/icon.h>
#include <nx/vms/rules/utils/event_details.h>
#include <nx/vms/text/human_readable.h>

#include "../utils/field.h"
#include "../utils/type.h"

namespace {
bool isValidReason(nx::vms::api::EventReason reason)
{
    switch (reason)
    {
        case nx::vms::api::EventReason::failedToConnectToLdap:
        case nx::vms::api::EventReason::failedToCompleteSyncWithLdap:
        case nx::vms::api::EventReason::noLdapUsersAfterSync:
        case nx::vms::api::EventReason::someUsersNotFoundInLdap:
            return true;

        default:
            return false;
    }
}
} // namespace

namespace nx::vms::rules {

LdapSyncIssueEvent::LdapSyncIssueEvent(std::chrono::microseconds timestamp,
    vms::api::EventReason reasonCode,
    std::chrono::seconds syncInterval,
    nx::Uuid serverId)
    :
    BasicEvent(timestamp),
    m_reasonCode(reasonCode),
    m_syncInterval(syncInterval),
    m_serverId(serverId)
{
}

QString LdapSyncIssueEvent::resourceKey() const
{
    return {};
}

QString LdapSyncIssueEvent::uniqueName() const
{
    return utils::makeName(
        BasicEvent::uniqueName(), QString::number(static_cast<int>(reasonCode())));
}

void LdapSyncIssueEvent::fillAggregationInfo(const AggregatedEventPtr& aggregatedEvent)
{
    Reasons reasonsMap;

    for (const auto& event: aggregatedEvent->aggregatedEvents())
    {
        const auto ldapEvent = event.dynamicCast<LdapSyncIssueEvent>();
        if (!NX_ASSERT(ldapEvent, "Unexpected type of event: %1", event->type()))
            continue;

        const auto reason = ldapEvent->reasonCode();
        if (!NX_ASSERT(isValidReason(reason), "Unexpected reasonCode value: %1", reason))
            continue;

        reasonsMap[reason] += 1;
    }

    setCountByReasons(reasonsMap);
}

QVariantMap LdapSyncIssueEvent::details(common::SystemContext* context) const
{
    auto result = BasicEvent::details(context);
    utils::insertLevel(result, nx::vms::event::Level::important);
    utils::insertIcon(result, nx::vms::rules::Icon::networkIssue);
    utils::insertIfNotEmpty(result, utils::kDetailingDetailName, reasonText());
    return result;
}

const ItemDescriptor& LdapSyncIssueEvent::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = utils::type<LdapSyncIssueEvent>(),
        .displayName = NX_DYNAMIC_TRANSLATABLE(tr("LDAP Sync Issue Event")),
        .description = {},
        .flags = {ItemFlag::instant, ItemFlag::aggregationByTypeSupported},
        .fields = {},
        .emailTemplatePath = ":/email_templates/ldap_sync_issue.mustache"
    };
    return kDescriptor;
}

QString LdapSyncIssueEvent::reasonText() const
{
    QStringList result;
    for (auto [reason, count]: m_countByReasons.asKeyValueRange())
    {
        result +=
            tr("%1 (%n times)", "%1 is description of event. Will be replaced in runtime", count)
                .arg(ldapSyncIssueReason(reason));
    }

    return result.size() > 1 ? result.join(", ") : ldapSyncIssueReason(m_reasonCode);
}

QString LdapSyncIssueEvent::ldapSyncIssueReason(vms::api::EventReason reasonCode) const
{
    switch (reasonCode)
    {
        case api::EventReason::failedToConnectToLdap:
            return tr("Failed to connect to the LDAP server.");

        case api::EventReason::failedToCompleteSyncWithLdap:
            return tr("Failed to complete the sync within a %1 timeout.",
                "Timeout duration in human-readable form (ex.: 1 minute)")
                .arg(text::HumanReadable::timeSpan(m_syncInterval));

        case api::EventReason::noLdapUsersAfterSync:
            return tr("No user accounts on LDAP server match the synchronization settings.");

        case api::EventReason::someUsersNotFoundInLdap:
            return tr("Some LDAP users or groups were not found in the LDAP database.");

        default:
            NX_ASSERT(false, "Unexpected switch value: %1", reasonCode);
        return {};
    }
}

} // namespace nx::vms::rules
