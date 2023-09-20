// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "ldap_sync_issue_event.h"

#include <core/resource/media_server_resource.h>
#include <nx/reflect/json.h>
#include <nx/vms/event/aggregation_info.h>

namespace nx::vms::event {

LdapSyncIssueEvent::LdapSyncIssueEvent(
    vms::api::EventReason reason,
    std::chrono::microseconds timestamp,
    std::optional<std::chrono::seconds> syncInterval)
    :
    ReasonedEvent(EventType::ldapSyncIssueEvent, {}, timestamp.count(), reason, {}),
    m_syncInterval(syncInterval)
{
}

EventParameters LdapSyncIssueEvent::getRuntimeParams() const
{
    auto result = base_type::getRuntimeParams();
    result.analyticsPluginId = m_syncInterval
        ? QString::number(m_syncInterval->count())
        : "";
    return result;
}

bool LdapSyncIssueEvent::isValidReason(vms::api::EventReason reason)
{
    switch (reason)
    {
        case vms::api::EventReason::failedToConnectToLdap:
        case vms::api::EventReason::failedToCompleteSyncWithLdap:
        case vms::api::EventReason::noLdapUsersAfterSync:
        case vms::api::EventReason::someUsersNotFoundInLdap:
            return true;

        default:
            return false;
    }
}

bool LdapSyncIssueEvent::isAggregatedEvent(const EventParameters& params)
{
    return !params.inputPortId.isEmpty();
}

LdapSyncIssueEvent::Reasons LdapSyncIssueEvent::decodeReasons(const EventParameters& params)
{
    Reasons result;

    if (nx::reflect::json::deserialize(params.inputPortId.toStdString(), &result))
    {
        auto it = result.begin();
        while (it != result.end())
        {
            if (isValidReason(it->first))
                it++;
            else
                it = result.erase(it);
        }

        return result;
    }

    return {};
}

void LdapSyncIssueEvent::encodeReasons(const AggregationInfo& info, EventParameters& target)
{
    Reasons reasonsMap;

    for (const auto& infoDetail: info.toList())
    {
        auto reason = infoDetail.runtimeParams().reasonCode;
        if (isValidReason(reason))
            reasonsMap[reason] += infoDetail.count();
    }

    target.inputPortId = QString::fromStdString(nx::reflect::json::serialize(reasonsMap));
}

std::optional<std::chrono::seconds> LdapSyncIssueEvent::syncInterval(const EventParameters& params)
{
    bool ok{false};
    auto value = params.analyticsPluginId.toInt(&ok);

    if (ok)
        return std::chrono::seconds(value);

    return {};
}

} // namespace nx::vms::event
