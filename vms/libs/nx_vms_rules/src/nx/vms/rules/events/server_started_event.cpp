// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "server_started_event.h"

#include "../utils/event_details.h"
#include "../utils/string_helper.h"
#include "../utils/type.h"

namespace nx::vms::rules {

ServerStartedEvent::ServerStartedEvent(std::chrono::microseconds timestamp, QnUuid serverId):
    base_type(timestamp),
    m_serverId(serverId)
{
}

QVariantMap ServerStartedEvent::details(common::SystemContext* context) const
{
    auto result = BasicEvent::details(context);

    utils::insertIfNotEmpty(result, utils::kExtendedCaptionDetailName, extendedCaption(context));
    result.insert(utils::kEmailTemplatePathDetailName, manifest().emailTemplatePath);

    return result;
}

QString ServerStartedEvent::extendedCaption(common::SystemContext* context) const
{
    const auto resourceName = utils::StringHelper(context).resource(m_serverId, Qn::RI_WithUrl);
    return tr("Server \"%1\" Started").arg(resourceName);
}

const ItemDescriptor& ServerStartedEvent::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = utils::type<ServerStartedEvent>(),
        .displayName = tr("Server Started"),
        .description = "",
        .emailTemplatePath = ":/email_templates/mediaserver_started.mustache"
    };
    return kDescriptor;
}

} // namespace nx::vms::rules
