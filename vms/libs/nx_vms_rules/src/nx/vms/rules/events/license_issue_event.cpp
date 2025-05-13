// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "license_issue_event.h"

#include <core/resource/camera_resource.h>
#include <core/resource/device_dependent_strings.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/log/assert.h>
#include <nx/vms/common/html/html.h>
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

QVariantMap LicenseIssueEvent::details(
    common::SystemContext* context,
    Qn::ResourceInfoLevel detailLevel) const
{
    auto result = BasicEvent::details(context, detailLevel);
    fillAggregationDetailsForServer(result, context, serverId(), detailLevel);

    utils::insertLevel(result, nx::vms::event::Level::critical);
    utils::insertIcon(result, nx::vms::rules::Icon::license);
    utils::insertClientAction(result, nx::vms::rules::ClientAction::licensesSettings);

    result[utils::kCaptionDetailName] = manifest().displayName();

    auto [text, devices] = detailing(context);

    QStringList details{{text}};
    static const QString kRow = "- %1";
    for (const auto& device: devices)
        details.push_back(kRow.arg(device));
    result[utils::kDetailingDetailName] = details;
    result[utils::kDescriptionDetailName] = details.join('\n');

    result[utils::kHtmlDetailsName] = QStringList{{
        text,
        devices.join(common::html::kLineBreak)
    }};

    return result;
}

QString LicenseIssueEvent::extendedCaption(common::SystemContext* context,
    Qn::ResourceInfoLevel detailLevel) const
{
    const auto resourceName = Strings::resource(context, serverId(), detailLevel);
    return tr("Not enough licenses on %1", "Server name will be substituted").arg(resourceName);
}

std::pair<QString, QStringList> LicenseIssueEvent::detailing(
    nx::vms::common::SystemContext* context) const
{
    const auto devices =
        context->resourcePool()->getResourcesByIds<QnVirtualCameraResource>(m_deviceIds);
    const QString text = QnDeviceDependentStrings::getNameFromSet(
        context->resourcePool(),
        {
            tr("Recording has been disabled on the following devices:"),
            tr("Recording has been disabled on the following cameras:")
        },
        devices);

    QStringList deviceNames;
    for (const auto& device: devices)
        deviceNames.push_back(Strings::resource(device, Qn::RI_WithUrl));
    deviceNames.sort();

    return std::make_pair(text, deviceNames);
}

const ItemDescriptor& LicenseIssueEvent::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = utils::type<LicenseIssueEvent>(),
        .groupId = kServerIssueEventGroup,
        .displayName = NX_DYNAMIC_TRANSLATABLE(tr("License Issue")),
        .description = "Triggered when a trial license expires or when "
            "the server with activated licenses goes offline.",
        .flags = {ItemFlag::instant, ItemFlag::localLicense},
        .resources = {
            {utils::kDeviceIdsFieldName, {ResourceType::device, Qn::ViewContentPermission}},
            {utils::kServerIdFieldName, {ResourceType::server}}}
    };
    return kDescriptor;
}

} // namespace nx::vms::rules
