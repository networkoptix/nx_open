// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "server_started_event.h"

#include "../group.h"
#include "../strings.h"
#include "../utils/event_details.h"
#include "../utils/field.h"
#include "../utils/type.h"

namespace nx::vms::rules {

ServerStartedEvent::ServerStartedEvent(std::chrono::microseconds timestamp, nx::Uuid serverId):
    base_type(timestamp),
    m_serverId(serverId)
{
}

QString ServerStartedEvent::resourceKey() const
{
    return m_serverId.toSimpleString();
}

QVariantMap ServerStartedEvent::details(
    common::SystemContext* context, const nx::vms::api::rules::PropertyMap& aggregatedInfo) const
{
    auto result = BasicEvent::details(context, aggregatedInfo);

    utils::insertIfNotEmpty(result, utils::kExtendedCaptionDetailName, extendedCaption(context));
    utils::insertLevel(result, nx::vms::event::Level::common);
    utils::insertIcon(result, nx::vms::rules::Icon::server);
    utils::insertClientAction(result, nx::vms::rules::ClientAction::serverSettings);

    return result;
}

QString ServerStartedEvent::extendedCaption(common::SystemContext* context) const
{
    const auto resourceName = Strings::resource(context, m_serverId, Qn::RI_WithUrl);
    return tr("Server \"%1\" Started").arg(resourceName);
}

const ItemDescriptor& ServerStartedEvent::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = utils::type<ServerStartedEvent>(),
        .groupId = kServerIssueEventGroup,
        .displayName = NX_DYNAMIC_TRANSLATABLE(tr("Server Started")),
        .description = "Triggered when any server registered in the server starts.",
        .resources = {{utils::kServerIdFieldName, {ResourceType::server}}},
        .readPermissions = GlobalPermission::powerUser,
        .emailTemplatePath = ":/email_templates/mediaserver_started.mustache"
    };
    return kDescriptor;
}

} // namespace nx::vms::rules
