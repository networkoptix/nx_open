// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "storage_issue_event.h"

#include "../utils/event_details.h"
#include "../utils/string_helper.h"

namespace nx::vms::rules {

QVariantMap StorageIssueEvent::details(common::SystemContext* context) const
{
    auto result = ReasonedEvent::details(context);

    utils::insertIfNotEmpty(result, utils::kExtendedCaptionDetailName, extendedCaption(context));
    result.insert(utils::kEmailTemplatePathDetailName, manifest().emailTemplatePath);

    return result;
}

QString StorageIssueEvent::extendedCaption(common::SystemContext* context) const
{
    const auto resourceName = utils::StringHelper(context).resource(serverId(), Qn::RI_WithUrl);
    return tr("Storage Issue at %1").arg(resourceName);
}

const ItemDescriptor& StorageIssueEvent::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = "nx.events.storageIssue",
        .displayName = tr("Storage Issue"),
        .description = "",
        .emailTemplatePath = ":/email_templates/storage_failure.mustache"
    };
    return kDescriptor;
}

} // namespace nx::vms::rules
