// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "fan_error_event.h"

#include <core/resource_management/resource_pool.h>
#include <nx/vms/common/system_context.h>

#include "../event_filter_fields/source_server_field.h"
#include "../group.h"
#include "../strings.h"
#include "../utils/event_details.h"
#include "../utils/field.h"
#include "../utils/type.h"

namespace nx::vms::rules {

FanErrorEvent::FanErrorEvent(std::chrono::microseconds timestamp, nx::Uuid serverId):
    BasicEvent(timestamp),
    m_serverId(serverId)
{
}

QVariantMap FanErrorEvent::details(
    common::SystemContext* context,
    Qn::ResourceInfoLevel detailLevel) const
{
    auto result = BasicEvent::details(context, detailLevel);
    fillAggregationDetailsForServer(result, context, serverId(), detailLevel);

    utils::insertLevel(result, nx::vms::event::Level::critical);
    utils::insertClientAction(result, nx::vms::rules::ClientAction::poeSettings);
    utils::insertIcon(result, nx::vms::rules::Icon::fanError);

    result[utils::kCaptionDetailName] = manifest().displayName();

    return result;
}

QString FanErrorEvent::extendedCaption(common::SystemContext* context,
    Qn::ResourceInfoLevel detailLevel) const
{
    const auto resourceName = Strings::resource(context, serverId(), detailLevel);
    return tr("Fan failure at %1").arg(resourceName);
}

const ItemDescriptor& FanErrorEvent::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = utils::type<FanErrorEvent>(),
        .groupId = kServerIssueEventGroup,
        .displayName = NX_DYNAMIC_TRANSLATABLE(tr("Fan Failure")),
        .description = "Triggered when a fan failure is detected on the server.",
        .fields = {
            makeFieldDescriptor<SourceServerField>(
                utils::kServerIdFieldName,
                Strings::at(),
                {},
                ResourceFilterFieldProperties{
                    .base = FieldProperties{.optional = false},
                    .validationPolicy = kHasFanMonitoringValidationPolicy
                }.toVariantMap()),
        },
        .resources = {{utils::kServerIdFieldName, {ResourceType::server}}},
        .readPermissions = GlobalPermission::powerUser,
        .serverFlags = {api::ServerFlag::SF_HasFanMonitoringCapability}
    };
    return kDescriptor;
}

} // namespace nx::vms::rules
