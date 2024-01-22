// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "network_issue_event.h"

#include <core/resource/camera_resource.h>
#include <core/resource/device_dependent_strings.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/common/system_context.h>

#include "../group.h"
#include "../utils/event_details.h"
#include "../utils/field.h"
#include "../utils/string_helper.h"
#include "../utils/type.h"

namespace nx::vms::rules {

NetworkIssueEvent::NetworkIssueEvent(
    std::chrono::microseconds timestamp,
    QnUuid cameraId,
    nx::vms::api::EventReason reason,
    const NetworkIssueInfo& info)
    :
    BasicEvent(timestamp),
    m_cameraId(cameraId),
    m_reason(reason),
    m_info(info)
{
}

QString NetworkIssueEvent::resourceKey() const
{
    return m_cameraId.toSimpleString();
}

QString NetworkIssueEvent::uniqueName() const
{
    return utils::makeName(
        BasicEvent::uniqueName(),
        cameraId().toSimpleString(),
        QString::number(static_cast<int>(reason())));
}

QVariantMap NetworkIssueEvent::details(common::SystemContext* context) const
{
    auto result = BasicEvent::details(context);

    utils::insertIfNotEmpty(result, utils::kExtendedCaptionDetailName, extendedCaption(context));
    utils::insertIfNotEmpty(result, utils::kReasonDetailName, reason(context));
    result.insert(utils::kEmailTemplatePathDetailName, manifest().emailTemplatePath);
    utils::insertLevel(result, nx::vms::event::Level::important);
    utils::insertIcon(result, nx::vms::rules::Icon::connection);
    utils::insertClientAction(result, nx::vms::rules::ClientAction::cameraSettings);

    return result;
}

QString NetworkIssueEvent::extendedCaption(common::SystemContext* context) const
{
    const auto resourceName = utils::StringHelper(context).resource(cameraId(), Qn::RI_WithUrl);
    return tr("Network Issue at %1").arg(resourceName);
}

QString NetworkIssueEvent::reason(common::SystemContext* context) const
{
    using nx::vms::api::EventReason;
    using namespace std::chrono;

    switch (reason())
    {
        case EventReason::networkNoFrame:
        {
            const int secs = duration_cast<seconds>(info().timeout).count();
            return tr("No data received during last %n seconds.", "", secs);
        }
        case EventReason::networkRtpStreamError:
        {
            return info().isPrimaryStream()
                ? tr("RTP error in primary stream (%1).").arg(info().message)
                : tr("RTP error in secondary stream (%1).").arg(info().message);
        }
        case EventReason::networkConnectionClosed:
        {
            const auto device = context->resourcePool()
                ->getResourceById<QnVirtualCameraResource>(cameraId());
            const auto deviceType = device
                ? QnDeviceDependentStrings::calculateDeviceType(context->resourcePool(), {device})
                : QnCameraDeviceType::Mixed;

            if (deviceType == QnCameraDeviceType::Camera)
            {
                return info().isPrimaryStream()
                    ? tr("Connection to camera (primary stream) was unexpectedly closed.")
                    : tr("Connection to camera (secondary stream) was unexpectedly closed.");
            }
            else
            {
                return tr("Connection to device was unexpectedly closed.");
            }
        }
        case EventReason::networkRtpPacketLoss:
        {
            return tr("RTP packet loss detected.");
        }
        case EventReason::networkBadCameraTime:
        {
            return tr("Failed to force using camera time, as it lags too much."
                " System time will be used instead.");
        }
        case EventReason::networkCameraTimeBackToNormal:
        {
            return tr("Camera time is back to normal.");
        }
        case EventReason::networkNoResponseFromDevice:
        {
            return tr("Device does not respond to network requests.");
        }
        case EventReason::networkMulticastAddressConflict:
        {
            auto addressLine = info().isPrimaryStream()
                ? tr("Address %1 is already in use by %2 on primary stream.",
                    "%1 is the address, %2 is the device name")
                : tr("Address %1 is already in use by %2 on secondary stream.",
                    "%1 is the address, %2 is the device name");

            return tr("Multicast address conflict detected.") + " "
                + addressLine
                    .arg(QString::fromStdString(info().address.toString()))
                    .arg(info().deviceName);
        }
        case EventReason::networkMulticastAddressIsInvalid:
        {
            return tr("Network address %1 is not a multicast address.")
                .arg(QString::fromStdString(info().address.toString()));
        }
        default:
            return QString();
    }
}

const ItemDescriptor& NetworkIssueEvent::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = utils::type<NetworkIssueEvent>(),
        .groupId = kDeviceIssueEventGroup,
        .displayName = tr("Network Issue"),
        .flags = {ItemFlag::instant, ItemFlag::aggregationByTypeSupported},
        .permissions = {
            .resourcePermissions = {{utils::kCameraIdFieldName, Qn::ViewContentPermission}}
        },
        .emailTemplatePath = ":/email_templates/network_issue.mustache"
    };
    return kDescriptor;
}

} // namespace nx::vms::rules
