// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "fan_error_event.h"

#include "../event_filter_fields/source_server_field.h"
#include "../group.h"
#include "../strings.h"
#include "../utils/event_details.h"
#include "../utils/field.h"
#include "../utils/type.h"

namespace nx::vms::rules {

FanErrorEvent::FanErrorEvent(std::chrono::microseconds timestamp, nx::Uuid serverId):
    base_type(timestamp),
    m_serverId(serverId)
{
}

QString FanErrorEvent::resourceKey() const
{
    return m_serverId.toSimpleString();
}

QVariantMap FanErrorEvent::details(
    common::SystemContext* context, const nx::vms::api::rules::PropertyMap& aggregatedInfo) const
{
    auto result = BasicEvent::details(context, aggregatedInfo);

    utils::insertIfNotEmpty(result, utils::kExtendedCaptionDetailName, extendedCaption(context));
    utils::insertLevel(result, nx::vms::event::Level::critical);
    utils::insertClientAction(result, nx::vms::rules::ClientAction::poeSettings);
    utils::insertIcon(result, nx::vms::rules::Icon::fanError);

    return result;
}

QString FanErrorEvent::extendedCaption(common::SystemContext* context) const
{
    const auto resourceName = Strings::resource(context, m_serverId, Qn::RI_WithUrl);
    return tr("Fan error at %1").arg(resourceName);
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
        .emailTemplateName = "timestamp_only.mustache",
        .serverFlags = {api::ServerFlag::SF_HasFanMonitoringCapability}
    };
    return kDescriptor;
}

} // namespace nx::vms::rules
