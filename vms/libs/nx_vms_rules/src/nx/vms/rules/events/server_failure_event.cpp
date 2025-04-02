// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "server_failure_event.h"

#include "../group.h"
#include "../strings.h"
#include "../utils/event_details.h"
#include "../utils/field.h"
#include "../utils/type.h"

namespace nx::vms::rules {

ServerFailureEvent::ServerFailureEvent(
    std::chrono::microseconds timestamp,
    nx::Uuid serverId,
    nx::vms::api::EventReason reason)
    :
    BasicEvent(timestamp),
    m_serverId(serverId),
    m_reason(reason)
{
}

QVariantMap ServerFailureEvent::details(
    common::SystemContext* context,
    const nx::vms::api::rules::PropertyMap& aggregatedInfo,
    Qn::ResourceInfoLevel detailLevel) const
{
    auto result = BasicEvent::details(context, aggregatedInfo, detailLevel);
    fillAggregationDetailsForServer(result, context, serverId(), detailLevel);

    utils::insertLevel(result, nx::vms::event::Level::critical);
    utils::insertIcon(result, nx::vms::rules::Icon::server);
    utils::insertClientAction(result, nx::vms::rules::ClientAction::serverSettings);

    const QString detailing = reason(context);
    result[utils::kDetailingDetailName] = detailing;
    result[utils::kHtmlDetailsName] = detailing;

    return result;
}

QString ServerFailureEvent::extendedCaption(common::SystemContext* context,
    Qn::ResourceInfoLevel detailLevel) const
{
    const auto resourceName = Strings::resource(context, serverId(), detailLevel);
    return tr("%1 Failure").arg(resourceName);
}

QString ServerFailureEvent::reason(common::SystemContext* /*context*/) const
{
    using nx::vms::api::EventReason;

    switch (reason())
    {
        case EventReason::serverTerminated: //< TODO: #sivanov Change the enum value name.
            return tr("Connection to server is lost.");

        case EventReason::serverStarted: //< TODO: #sivanov Change the enum value name.
            return tr("Server stopped unexpectedly.");

        default:
            NX_ASSERT(0, "Unexpected reason");
            return {};
    }
}

const ItemDescriptor& ServerFailureEvent::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = utils::type<ServerFailureEvent>(),
        .groupId = kServerIssueEventGroup,
        .displayName = NX_DYNAMIC_TRANSLATABLE(tr("Server Failure")),
        .description = "Triggered when a server goes down due to hardware failure, "
            "software issue, or manual/emergency shutdown.",
        .resources = {{utils::kServerIdFieldName, {ResourceType::server}}},
        .readPermissions = GlobalPermission::powerUser
    };
    return kDescriptor;
}

} // namespace nx::vms::rules
