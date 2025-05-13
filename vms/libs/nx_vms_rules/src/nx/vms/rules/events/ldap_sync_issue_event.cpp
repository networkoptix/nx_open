// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "ldap_sync_issue_event.h"

#include <chrono>

#include <core/resource_management/resource_pool.h>
#include <nx/fusion/serialization/json_functions.h>
#include <nx/vms/common/system_context.h>
#include <nx/vms/rules/aggregated_event.h>
#include <nx/vms/rules/icon.h>
#include <nx/vms/rules/utils/event_details.h>
#include <nx/vms/text/human_readable.h>

#include "../utils/field.h"
#include "../utils/type.h"

namespace {

static constexpr auto kReasonToCount = "reasonToCount";

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

LdapSyncIssueEvent::LdapSyncIssueEvent(
    std::chrono::microseconds timestamp,
    vms::api::EventReason reason,
    std::chrono::seconds syncInterval,
    nx::Uuid serverId)
    :
    BasicEvent(timestamp),
    m_serverId(serverId),
    m_reason(reason),
    m_syncInterval(syncInterval)
{
}

QVariantMap LdapSyncIssueEvent::details(
    common::SystemContext* context,
    Qn::ResourceInfoLevel detailLevel) const
{
    auto result = BasicEvent::details(context, detailLevel);
    fillAggregationDetailsForServer(result, context, serverId(), detailLevel);

    utils::insertLevel(result, nx::vms::event::Level::important);
    utils::insertIcon(result, nx::vms::rules::Icon::server);

    result[utils::kCaptionDetailName] = manifest().displayName();

    const QString detailing = reasonText(m_reason);
    result[utils::kDescriptionDetailName] = detailing;
    result[utils::kDetailingDetailName] = detailing;
    result[utils::kHtmlDetailsName] = detailing;

    return result;
}

nx::vms::api::rules::PropertyMap LdapSyncIssueEvent::aggregatedInfo(
    const AggregatedEvent& aggregatedEvent) const
{
    nx::vms::api::rules::PropertyMap aggregatedInfo;
    std::map<vms::api::EventReason, int> reasonToCount;
    NX_ASSERT(!aggregatedEvent.aggregatedEvents().empty(), "Aggregated event cannot be empty.");
    for (const auto& event: aggregatedEvent.aggregatedEvents())
    {
        const auto ldapSyncIssueEvent = event.dynamicCast<LdapSyncIssueEvent>();
        if (!NX_ASSERT(ldapSyncIssueEvent, "Unexpected type of event: %1.", event->type()))
            continue;

        const auto reason = ldapSyncIssueEvent->reason();
        if (!NX_ASSERT(isValidReason(reason), "Unexpected reason value: %1.", reason))
            continue;

        reasonToCount[reason]++;
    }

    NX_ASSERT(!reasonToCount.empty(), "At least one cause of an event is expected: %1.", type());

    if (reasonToCount.size() == 1 && reasonToCount.begin()->second == 1)
        return aggregatedInfo;

    QnJsonContext context;
    context.setSerializeMapToObject(true);
    QJson::serialize(&context, reasonToCount, &aggregatedInfo[kReasonToCount]);

    return aggregatedInfo;
}

QString LdapSyncIssueEvent::extendedCaption(common::SystemContext* context,
    Qn::ResourceInfoLevel detailLevel) const
{
    const auto resourceName = Strings::resource(context, serverId(), detailLevel);
    return tr("LDAP Sync Issue at %1").arg(resourceName);
}

QString LdapSyncIssueEvent::reasonText(
    const nx::vms::api::rules::PropertyMap& aggregatedInfo) const
{
    if (aggregatedInfo.isEmpty())
        return reasonText(m_reason);

    std::map<vms::api::EventReason, int> data;
    if (!NX_ASSERT(QJson::deserialize(aggregatedInfo[kReasonToCount], &data) && !data.empty()))
        return reasonText(m_reason);

    QStringList reasons;
    for (const auto& [reason, count]: data)
        reasons << tr("%1 (%n times)", "%1 is a cause of the event", count).arg(reasonText(reason));

    return reasons.join(", ");
}

QString LdapSyncIssueEvent::reasonText(vms::api::EventReason reason) const
{
    switch (reason)
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
            NX_ASSERT(false, "Unexpected reason: %1", (int) reason);
            return {};
    }
}

const ItemDescriptor& LdapSyncIssueEvent::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = utils::type<LdapSyncIssueEvent>(),
        .displayName = NX_DYNAMIC_TRANSLATABLE(tr("LDAP Sync Issue")),
        .description = "Triggered when the LDAP server fails to synchronize with the site.",
        .flags = {ItemFlag::instant}
    };
    return kDescriptor;
}

} // namespace nx::vms::rules
