// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "license_issue_event.h"

#include "../utils/event_details.h"
#include "../utils/string_helper.h"

namespace nx::vms::rules {

QString LicenseIssueEvent::uniqueName() const
{
    // Licence issue event does not require uniqness offered by the ReasonedEvent::uniqueName().
    return BasicEvent::uniqueName();
}

QVariantMap LicenseIssueEvent::details(common::SystemContext* context) const
{
    auto result = BasicEvent::details(context);

    utils::insertIfNotEmpty(result, utils::kExtendedCaptionDetailName, extendedCaption(context));
    result.insert(utils::kEmailTemplatePathDetailName, manifest().emailTemplatePath);

    return result;
}

QString LicenseIssueEvent::extendedCaption(common::SystemContext* context) const
{
    if (totalEventCount() == 1)
    {
        const auto resourceName = utils::StringHelper(context).resource(serverId(), Qn::RI_WithUrl);
        return tr("Server \"%1\" has a license problem").arg(resourceName);
    }

    return BasicEvent::extendedCaption();
}

const ItemDescriptor& LicenseIssueEvent::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = "nx.events.licenseIssue",
        .displayName = tr("License Issue"),
        .description = "",
        .emailTemplatePath = ":/email_templates/license_issue.mustache"
    };
    return kDescriptor;
}

} // namespace nx::vms::rules
