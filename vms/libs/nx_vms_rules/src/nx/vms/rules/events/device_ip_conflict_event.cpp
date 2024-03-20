// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "device_ip_conflict_event.h"

#include <QtNetwork/QHostAddress>

#include <core/resource/device_dependent_strings.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/common/html/html.h>
#include <nx/vms/common/system_context.h>

#include "../group.h"
#include "../utils/event_details.h"
#include "../utils/field.h"
#include "../utils/string_helper.h"
#include "../utils/type.h"

namespace nx::vms::rules {

DeviceIpConflictEvent::DeviceIpConflictEvent(
    std::chrono::microseconds timestamp,
    nx::Uuid serverId,
    UuidList deviceIds,
    const QHostAddress& address,
    const QStringList& macAddrList)
    :
    base_type(timestamp),
    m_serverId(serverId),
    m_deviceIds(deviceIds),
    m_ipAddress(address.toString()),
    m_macAddresses(macAddrList)
{
}

QString DeviceIpConflictEvent::resourceKey() const
{
    return m_serverId.toSimpleString();
}

QVariantMap DeviceIpConflictEvent::details(common::SystemContext* context) const
{
    auto result = BasicEvent::details(context);

    utils::insertIfNotEmpty(result, utils::kCaptionDetailName, caption(context));
    utils::insertIfNotEmpty(result, utils::kDetailingDetailName, detailing());
    utils::insertIfNotEmpty(result, utils::kExtendedCaptionDetailName, extendedCaption(context));
    utils::insertIfNotEmpty(result, utils::kNameDetailName, name(context));
    result.insert(utils::kEmailTemplatePathDetailName, manifest().emailTemplatePath);
    utils::insertLevel(result, nx::vms::event::Level::important);
    utils::insertIcon(result, nx::vms::rules::Icon::connection);
    utils::insertClientAction(result, nx::vms::rules::ClientAction::browseUrl);
    utils::insertIfNotEmpty(result, utils::kUrlDetailName, m_ipAddress);

    return result;
}

QString DeviceIpConflictEvent::caption(common::SystemContext* context) const
{
    // TODO: #amalov The number of devices in translation should be corrected in 5.1+
    return QnDeviceDependentStrings::getDefaultNameFromSet(context->resourcePool(),
        tr("Device IP Conflict", "", 1),
        tr("Camera IP Conflict", "", 1));
}

QStringList DeviceIpConflictEvent::detailing() const
{
    QStringList result;

    result << tr("Conflicting Address: %1").arg(m_ipAddress);
    int n = 0;
    for (const auto& macAddress: m_macAddresses)
        result << tr("MAC #%1: %2").arg(++n).arg(macAddress);

    return result;
}

QString DeviceIpConflictEvent::extendedCaption(common::SystemContext* context) const
{
    const auto resourceName = utils::StringHelper(context).resource(serverId(), Qn::RI_WithUrl);
    return QnDeviceDependentStrings::getDefaultNameFromSet(
        context->resourcePool(),
        tr("Device IP Conflict at %1", "Device IP Conflict at <server_name>"),
        tr("Camera IP Conflict at %1", "Camera IP Conflict at <server_name>")).arg(resourceName);
}

QString DeviceIpConflictEvent::name(common::SystemContext* context) const
{
    // TODO: #amalov The number of devices in translation should be corrected in 5.1+
    return QnDeviceDependentStrings::getDefaultNameFromSet(context->resourcePool(),
        tr("Device IP Conflict", "", 1),
        tr("Camera IP Conflict", "", 1));
}

const ItemDescriptor& DeviceIpConflictEvent::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = utils::type<DeviceIpConflictEvent>(),
        .groupId = kDeviceIssueEventGroup,
        .displayName = tr("Device IP Conflict"),
        .resources = {
            {utils::kDeviceIdsFieldName, {ResourceType::Device, Qn::WritePermission}},
            {utils::kServerIdFieldName, {ResourceType::Server}}},
        .emailTemplatePath = ":/email_templates/camera_ip_conflict.mustache"
    };
    return kDescriptor;
}

} // namespace nx::vms::rules
