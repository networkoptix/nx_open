// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "network_issue_event.h"

#include "../utils/event_details.h"
#include "../utils/string_helper.h"

namespace nx::vms::rules {

QVariantMap NetworkIssueEvent::details(common::SystemContext* context) const
{
    auto result = ReasonedEvent::details(context);

    utils::insertIfNotEmpty(result, utils::kExtendedCaptionDetailName, extendedCaption(context));
    result.insert(utils::kEmailTemplatePathDetailName, manifest().emailTemplatePath);

    return result;
}

QString NetworkIssueEvent::extendedCaption(common::SystemContext* context) const
{
    const auto resourceName = utils::StringHelper(context).resource(serverId(), Qn::RI_WithUrl);
    return tr("Network Issue at %1").arg(resourceName);
}

const ItemDescriptor& NetworkIssueEvent::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = "nx.events.networkIssue",
        .displayName = tr("Network Issue"),
        .description = "",
        .emailTemplatePath = ":/email_templates/network_issue.mustache"
    };
    return kDescriptor;
}

} // namespace nx::vms::rules
