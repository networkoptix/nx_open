// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "device_ip_conflict_event.h"

#include <QtNetwork/QHostAddress>

#include <core/resource/device_dependent_strings.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/common/html/html.h>
#include <nx/vms/common/system_context.h>

#include "../group.h"
#include "../strings.h"
#include "../utils/event_details.h"
#include "../utils/field.h"
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

QVariantMap DeviceIpConflictEvent::details(
    common::SystemContext* context, const nx::vms::api::rules::PropertyMap& aggregatedInfo) const
{
    auto result = BasicEvent::details(context, aggregatedInfo);

    result.insert(utils::kCaptionDetailName, caption(context));
    result.insert(utils::kDetailingDetailName, detailing());
    result.insert(utils::kExtendedCaptionDetailName, extendedCaption(context));
    result.insert(utils::kNameDetailName, name(context));
    result.insert(utils::kUrlDetailName, m_ipAddress);
    utils::insertLevel(result, nx::vms::event::Level::important);
    utils::insertIcon(result, nx::vms::rules::Icon::resource);
    utils::insertClientAction(result, nx::vms::rules::ClientAction::browseUrl);

    return result;
}

QString DeviceIpConflictEvent::caption(common::SystemContext* context) const
{
    return name(context);
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
    const auto resourceName = Strings::resource(context, serverId(), Qn::RI_WithUrl);
    return QnDeviceDependentStrings::getDefaultNameFromSet(
        context->resourcePool(),
        tr("Device IP Conflict at %1", "Device IP Conflict at <server_name>"),
        tr("Camera IP Conflict at %1", "Camera IP Conflict at <server_name>")).arg(resourceName);
}

QString DeviceIpConflictEvent::name(common::SystemContext* context)
{
    return QnDeviceDependentStrings::getDefaultNameFromSet(context->resourcePool(),
        tr("Device IP Conflict"),
        tr("Camera IP Conflict"));
}

ItemDescriptor DeviceIpConflictEvent::manifest(common::SystemContext* context)
{
    auto getDisplayName =
        [context = QPointer<common::SystemContext>(context)]
        {
            if (!context)
                return QString();

            return DeviceIpConflictEvent::name(context);
        };

    auto kDescriptor = ItemDescriptor{
        .id = utils::type<DeviceIpConflictEvent>(),
        .groupId = kDeviceIssueEventGroup,
        .displayName = TranslatableString(getDisplayName),
        .description = "Triggered when a conflict occurs due to another device entering "
            "the network with the same IP address, causing one of the devices to go offline",
        .resources = {
            {utils::kDeviceIdsFieldName, {ResourceType::device, Qn::WritePermission}},
            {utils::kServerIdFieldName, {ResourceType::server}}},
        .emailTemplateName = "timestamp_and_details.mustache"
    };
    return kDescriptor;
}

} // namespace nx::vms::rules
