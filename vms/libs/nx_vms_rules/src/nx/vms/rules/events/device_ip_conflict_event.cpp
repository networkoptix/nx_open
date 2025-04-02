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
    QList<DeviceInfo> devices,
    const QHostAddress& address)
    :
    BasicEvent(timestamp),
    m_serverId(serverId),
    m_ipAddress(address.toString())
{
    for (const auto& info: devices)
    {
        m_deviceIds.push_back(info.id);
        m_macAddresses.push_back(info.mac);
    }
}

QVariantMap DeviceIpConflictEvent::details(
    common::SystemContext* context,
    const nx::vms::api::rules::PropertyMap& aggregatedInfo,
    Qn::ResourceInfoLevel detailLevel) const
{
    auto result = BasicEvent::details(context, aggregatedInfo, detailLevel);
    fillAggregationDetailsForServer(result, context, serverId(), detailLevel);

    result.insert(utils::kCaptionDetailName, caption(context));
    result.insert(utils::kNameDetailName, name(context));
    result.insert(utils::kUrlDetailName, m_ipAddress);
    utils::insertLevel(result, nx::vms::event::Level::important);
    utils::insertIcon(result, nx::vms::rules::Icon::resource);
    utils::insertClientAction(result, nx::vms::rules::ClientAction::browseUrl);

    QStringList devices;
    for (const auto& id: m_deviceIds)
        devices.push_back(Strings::resource(context, id, detailLevel));

    result[utils::kDetailingDetailName] = detailing(context);
    result[utils::kHtmlDetailsName] = QStringList{{
        m_ipAddress,
        devices.join(common::html::kLineBreak),
        m_macAddresses.join(common::html::kLineBreak)
    }};

    return result;
}

QString DeviceIpConflictEvent::caption(common::SystemContext* context) const
{
    return name(context);
}

QStringList DeviceIpConflictEvent::detailing(common::SystemContext* context) const
{
    const QString row = QnDeviceDependentStrings::getDefaultNameFromSet(context->resourcePool(),
        tr("Device #%1: %2 (%3)", "Device #1: <device_name> (MAC address)"),
        tr("Camera #%1: %2 (%3)", "Camera #1: <device_name> (MAC address)"));

    QStringList result;

    result << tr("Conflicting Address: %1").arg(m_ipAddress);
    NX_ASSERT(m_deviceIds.size() == m_macAddresses.size(), "Consistency error");
    for (int i = 0; i < m_deviceIds.size() && i < m_macAddresses.size(); ++i)
    {
        const auto resourceName = Strings::resource(context, m_deviceIds[i], Qn::RI_NameOnly);
        result << row.arg(QString::number(i + 1), resourceName, m_macAddresses[i]);
    }

    return result;
}

QString DeviceIpConflictEvent::extendedCaption(common::SystemContext* context,
    Qn::ResourceInfoLevel detailLevel) const
{
    const auto resourceName = Strings::resource(context, serverId(), detailLevel);
    return QnDeviceDependentStrings::getDefaultNameFromSet(
        context->resourcePool(),
        tr("Device IP Conflict at %1", "Device IP Conflict at <server_name>"),
        tr("Camera IP Conflict at %1", "Camera IP Conflict at <server_name>")).arg(resourceName);
}

QString DeviceIpConflictEvent::name(common::SystemContext* context) const
{
    return manifest(context).displayName();
}

ItemDescriptor DeviceIpConflictEvent::manifest(common::SystemContext* context)
{
    auto getDisplayName =
        [context = QPointer<common::SystemContext>(context)]
        {
            if (!context)
                return QString();

            return QnDeviceDependentStrings::getDefaultNameFromSet(context->resourcePool(),
                tr("Device IP Conflict"),
                tr("Camera IP Conflict"));
        };

    auto kDescriptor = ItemDescriptor{
        .id = utils::type<DeviceIpConflictEvent>(),
        .groupId = kDeviceIssueEventGroup,
        .displayName = TranslatableString(getDisplayName),
        .description = "Triggered when a conflict occurs due to another device entering "
            "the network with the same IP address, causing one of the devices to go offline",
        .resources = {
            {utils::kDeviceIdsFieldName, {ResourceType::device, Qn::WritePermission}},
            {utils::kServerIdFieldName, {ResourceType::server}}}
    };
    return kDescriptor;
}

} // namespace nx::vms::rules
