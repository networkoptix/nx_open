// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "server_failure_event.h"

#include "../utils/event_details.h"
#include "../utils/string_helper.h"

namespace nx::vms::rules {

QVariantMap ServerFailureEvent::details(common::SystemContext* context) const
{
    auto result = ReasonedEvent::details(context);

    utils::insertIfNotEmpty(result, utils::kExtendedCaptionDetailName, extendedCaption(context));
    result.insert(utils::kEmailTemplatePathDetailName, manifest().emailTemplatePath);

    return result;
}

QString ServerFailureEvent::extendedCaption(common::SystemContext* context) const
{
    const auto resourceName = utils::StringHelper(context).resource(serverId(), Qn::RI_WithUrl);
    return tr("Server \"%1\" Failure").arg(resourceName);
}

const ItemDescriptor& ServerFailureEvent::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = "nx.events.serverFailure",
        .displayName = tr("Server Failure"),
        .description = "",
        .emailTemplatePath = ":/email_templates/mediaserver_failure.mustache"
    };
    return kDescriptor;
}

} // namespace nx::vms::rules
