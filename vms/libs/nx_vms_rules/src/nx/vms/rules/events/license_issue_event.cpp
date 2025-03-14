// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "license_issue_event.h"

#include <core/resource/camera_resource.h>
#include <core/resource/device_dependent_strings.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/log/assert.h>
#include <nx/vms/common/system_context.h>

#include "../group.h"
#include "../strings.h"
#include "../utils/event_details.h"
#include "../utils/field.h"
#include "../utils/type.h"

namespace nx::vms::rules {

LicenseIssueEvent::LicenseIssueEvent(
    std::chrono::microseconds timestamp,
    nx::Uuid serverId,
    const UuidSet& disabledCameras)
    :
    BasicEvent(timestamp),
    m_serverId(serverId),
    m_deviceIds(disabledCameras.values())
{
}

QString LicenseIssueEvent::resourceKey() const
{
    return m_serverId.toSimpleString();
}

QString LicenseIssueEvent::aggregationSubKey() const
{
    QStringList cameras;
    for (const auto id: m_deviceIds)
        cameras << id.toSimpleString();
    cameras.sort();
    return utils::makeKey(cameras);
}

QVariantMap LicenseIssueEvent::details(
    common::SystemContext* context, const nx::vms::api::rules::PropertyMap& aggregatedInfo) const
{
    auto result = BasicEvent::details(context, aggregatedInfo);

    result.insert(utils::kExtendedCaptionDetailName, extendedCaption(context));
    result.insert(utils::kReasonDetailName, reason(context));
    result.insert(utils::kDetailingDetailName, detailing(context));
    utils::insertLevel(result, nx::vms::event::Level::critical);
    utils::insertIcon(result, nx::vms::rules::Icon::license);
    utils::insertClientAction(result, nx::vms::rules::ClientAction::licensesSettings);

    return result;
}

QString LicenseIssueEvent::extendedCaption(common::SystemContext* context) const
{
    const auto resourceName = Strings::resource(context, serverId(), Qn::RI_WithUrl);
    return tr("%1 has a license problem", "Server name will be substituted").arg(resourceName);
}

const ItemDescriptor& LicenseIssueEvent::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = utils::type<LicenseIssueEvent>(),
        .groupId = kServerIssueEventGroup,
        .displayName = NX_DYNAMIC_TRANSLATABLE(tr("License Issue")),
        .description =  "Triggered when a trial license expires or when "
            "the server with activated licenses goes offline.",
        .flags = {ItemFlag::instant, ItemFlag::localLicense},
        .resources = {
            {utils::kDeviceIdsFieldName, {ResourceType::device, Qn::ViewContentPermission}},
            {utils::kServerIdFieldName, {ResourceType::server}}},
        .emailTemplateName = "timestamp_and_details.mustache"
    };
    return kDescriptor;
}

QString LicenseIssueEvent::reason(nx::vms::common::SystemContext* context) const
{
    QnVirtualCameraResourceList disabledCameras =
        context->resourcePool()->getResourcesByIds<QnVirtualCameraResource>(deviceIds());

    return QnDeviceDependentStrings::getNameFromSet(
        context->resourcePool(),
        QnCameraDeviceStringSet(
            tr("Not enough licenses. Recording has been disabled on the following devices:"),
            tr("Not enough licenses. Recording has been disabled on the following cameras:"),
            tr("Not enough licenses. Recording has been disabled on the following I/O modules:")),
        disabledCameras);
}

QStringList LicenseIssueEvent::detailing(nx::vms::common::SystemContext* context) const
{
    QnVirtualCameraResourceList disabledCameras =
        context->resourcePool()->getResourcesByIds<QnVirtualCameraResource>(deviceIds());

    if (!NX_ASSERT(!disabledCameras.isEmpty(),
        "At least one camera should be disabled on this event"))
    {
        return {};
    }

    QStringList result;
    for (const auto& camera: disabledCameras)
        result << Strings::resource(camera, Qn::RI_WithUrl);

    return result;
}

} // namespace nx::vms::rules
