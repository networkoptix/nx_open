// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "saas_issue_event.h"

#include <QtCore/QCollator>

#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>
#include <licensing/license.h>
#include <nx/utils/log/assert.h>
#include <nx/vms/common/system_context.h>
#include <nx/vms/event/strings_helper.h>

#include "../group.h"
#include "../strings.h"
#include "../utils/event_details.h"
#include "../utils/field.h"
#include "../utils/type.h"

namespace nx::vms::rules {

SaasIssueEvent::SaasIssueEvent(
    std::chrono::microseconds timestamp,
    nx::Uuid serverId,
    const QStringList& licenseKeys,
    nx::vms::api::EventReason reason)
    :
    BasicEvent(timestamp),
    m_serverId(serverId),
    m_licenseKeys(licenseKeys),
    m_reason(reason)
{
    NX_ASSERT(isLicenseMigrationIssue());
}

SaasIssueEvent::SaasIssueEvent(
    std::chrono::microseconds timestamp,
    nx::Uuid serverId,
    const UuidList& deviceIds,
    nx::vms::api::EventReason reason)
    :
    BasicEvent(timestamp),
    m_serverId(serverId),
    m_deviceIds(deviceIds),
    m_reason(reason)
{
    NX_ASSERT(!isLicenseMigrationIssue());
}

QString SaasIssueEvent::resourceKey() const
{
    return m_serverId.toSimpleString();
}

QVariantMap SaasIssueEvent::details(
    common::SystemContext* context, const nx::vms::api::rules::PropertyMap& aggregatedInfo) const
{
    auto result = BasicEvent::details(context, aggregatedInfo);

    result.insert(utils::kExtendedCaptionDetailName, extendedCaption());
    result.insert(utils::kReasonDetailName, reason(context));
    result.insert(utils::kDetailingDetailName, detailing(context));
    utils::insertLevel(result, nx::vms::event::Level::important);
    utils::insertIcon(result, nx::vms::rules::Icon::license);
    utils::insertClientAction(result, nx::vms::rules::ClientAction::licensesSettings);
    return result;
}

QString SaasIssueEvent::extendedCaption() const
{
    using namespace nx::vms::api;

    switch (m_reason)
    {
        case EventReason::licenseMigrationFailed:
        case EventReason::licenseMigrationSkipped:
             return tr("License migration issue");

        case EventReason::notEnoughLocalRecordingServices:
            return tr("Recording services disabled");

        case EventReason::notEnoughCloudRecordingServices:
            return tr("Cloud storage services disabled");

        case EventReason::notEnoughIntegrationServices:
            return tr("Paid integration services disabled");

        default:
            NX_ASSERT(false, "Unexpected event reason");
            return manifest().displayName;
    }
}

const ItemDescriptor& SaasIssueEvent::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = utils::type<SaasIssueEvent>(),
        .groupId = kServerIssueEventGroup,
        .displayName = NX_DYNAMIC_TRANSLATABLE(tr("Services Issue")),
        .description = "Triggered when there is a problem with one or more Services.",
        .flags = {ItemFlag::instant, ItemFlag::saasLicense},
        .resources = {
            {utils::kDeviceIdsFieldName, {ResourceType::device, Qn::ViewContentPermission}},
            {utils::kServerIdFieldName, {ResourceType::server}}},
        .emailTemplateName = "timestamp_and_details.mustache"
    };
    return kDescriptor;
}

QString SaasIssueEvent::licenseMigrationReason() const
{
    return nx::vms::event::StringsHelper::licenseMigrationReason(m_reason);
}

bool SaasIssueEvent::isLicenseMigrationIssue() const
{
    using namespace nx::vms::api;

    switch (m_reason)
    {
        case EventReason::licenseMigrationFailed:
        case EventReason::licenseMigrationSkipped:
            return true;

        case EventReason::notEnoughLocalRecordingServices:
        case EventReason::notEnoughCloudRecordingServices:
        case EventReason::notEnoughIntegrationServices:
            return false;

        default:
            NX_ASSERT(false, "Unhandled reason code");
            return false;
    }
}

QString SaasIssueEvent::reason(nx::vms::common::SystemContext* context) const
{
    return isLicenseMigrationIssue()
        ? licenseMigrationReason()
        : serviceDisabledReason(context);
}

QStringList SaasIssueEvent::detailing(nx::vms::common::SystemContext* context) const
{
    auto makeRow =
        [](const QString& text) { return QString(" - %1").arg(text); };

    QStringList result;
    if (isLicenseMigrationIssue())
    {
        for (const auto& key: m_licenseKeys)
        {
            if (const QnLicensePtr license = context->licensePool()->findLicense(key))
                result << makeRow(QnLicense::displayText(license->type(), license->cameraCount()));
            else
                result << makeRow(key);
        }
    }
    else
    {
        const auto devices =
            context->resourcePool()->getResourcesByIds<QnVirtualCameraResource>(m_deviceIds);
        for (const auto& device: devices)
            result << makeRow(Strings::resource(device, Qn::RI_WithUrl));
    }
    return result;
}

QString SaasIssueEvent::serviceDisabledReason(nx::vms::common::SystemContext* context) const
{
    using namespace nx::vms::api;

    const auto devices =
        context->resourcePool()->getResourcesByIds<QnVirtualCameraResource>(m_deviceIds);
    if (!NX_ASSERT(!devices.isEmpty(), "At least one device should be affected by this event"))
        return {};

    switch (m_reason)
    {
        case EventReason::notEnoughLocalRecordingServices:
            return tr("Recording on %n channels was stopped due to service overuse.",
                "", devices.count());

        case EventReason::notEnoughCloudRecordingServices:
            return tr("Cloud storage backup on %n channels was stopped due to service overuse.",
                "", devices.count());

        case EventReason::notEnoughIntegrationServices:
            return tr(
                "Paid integration service usage on %n channels was stopped due to service overuse.",
                    "", devices.count());

        default:
            NX_ASSERT(false, "Unexpected event reason");
            return {};
    }

    return {};
}

} // namespace nx::vms::rules
