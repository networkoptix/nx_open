// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "device_ip_conflict_event.h"

#include <QtNetwork/QHostAddress>

#include <core/resource/device_dependent_strings.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/common/html/html.h>
#include <nx/vms/common/system_context.h>

#include "../utils/event_details.h"
#include "../utils/string_helper.h"

namespace nx::vms::rules {

DeviceIpConflictEvent::DeviceIpConflictEvent(
    std::chrono::microseconds timestamp,
    QnUuid serverId,
    const QHostAddress& address,
    const QStringList& macAddrList)
    :
    base_type(timestamp),
    m_serverId(serverId),
    m_ipAddress(address.toString()),
    m_macAddresses(macAddrList)
{
}

QVariantMap DeviceIpConflictEvent::details(common::SystemContext* context) const
{
    auto result = BasicEvent::details(context);

    utils::insertIfNotEmpty(result, utils::kDetailingDetailName, detailing());
    utils::insertIfNotEmpty(result, utils::kExtendedCaptionDetailName, extendedCaption(context));
    result.insert(utils::kEmailTemplatePathDetailName, manifest().emailTemplatePath);

    return result;
}

QString DeviceIpConflictEvent::detailing() const
{
    QStringList result;

    result << tr("Conflicting Address: %1").arg(m_ipAddress);
    int n = 0;
    for (const auto& macAddress: m_macAddresses)
        result << tr("MAC #%1: %2").arg(++n).arg(macAddress);

    return result.join(common::html::kLineBreak);
}

QString DeviceIpConflictEvent::extendedCaption(common::SystemContext* context) const
{
    if (totalEventCount() == 1)
    {
        const auto resourceName = utils::StringHelper(context).resource(serverId(), Qn::RI_WithUrl);
        return QnDeviceDependentStrings::getDefaultNameFromSet(
            context->resourcePool(),
            tr("Device IP Conflict at %1", "Device IP Conflict at <server_name>"),
            tr("Camera IP Conflict at %1", "Camera IP Conflict at <server_name>")).arg(resourceName);
    }

    return BasicEvent::extendedCaption();
}

const ItemDescriptor& DeviceIpConflictEvent::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = "nx.events.deviceIpConflict",
        .displayName = tr("Device IP Conflict"),
        .description = "",
        .emailTemplatePath = ":/email_templates/camera_ip_conflict.mustache"
    };
    return kDescriptor;
}

} // namespace nx::vms::rules
