// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "license_issue_event.h"

#include <core/resource/camera_resource.h>
#include <core/resource/device_dependent_strings.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/log/assert.h>
#include <nx/vms/common/system_context.h>

#include "../utils/event_details.h"
#include "../utils/string_helper.h"

namespace nx::vms::rules {

LicenseIssueEvent::LicenseIssueEvent(
    std::chrono::microseconds timestamp,
    QnUuid serverId,
    const QnUuidSet& disabledCameras)
    :
    BasicEvent(timestamp),
    m_serverId(serverId),
    m_cameras(disabledCameras)
{
}

QVariantMap LicenseIssueEvent::details(common::SystemContext* context) const
{
    auto result = BasicEvent::details(context);

    utils::insertIfNotEmpty(result, utils::kExtendedCaptionDetailName, extendedCaption(context));
    utils::insertIfNotEmpty(result, utils::kDetailingDetailName, detailing(context));
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

QString LicenseIssueEvent::detailing(nx::vms::common::SystemContext* context) const
{
    QnVirtualCameraResourceList disabledCameras =
        context->resourcePool()->getResourcesByIds<QnVirtualCameraResource>(cameras());

    if (!NX_ASSERT(!disabledCameras.isEmpty(),
        "At least one camera should be disabled on this event"))
    {
        return {};
    }

    return QnDeviceDependentStrings::getNameFromSet(
        context->resourcePool(),
        QnCameraDeviceStringSet(
            tr("Not enough licenses. Recording has been disabled on following devices:"),
            tr("Not enough licenses. Recording has been disabled on following cameras:"),
            tr("Not enough licenses. Recording has been disabled on following I/O modules:")),
        disabledCameras);
}

} // namespace nx::vms::rules
