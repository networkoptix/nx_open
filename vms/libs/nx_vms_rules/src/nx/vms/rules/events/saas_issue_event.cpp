// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "saas_issue_event.h"

#include <QtCore/QCollator>

#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>
#include <licensing/license.h>
#include <nx/utils/log/assert.h>
#include <nx/vms/common/html/html.h>
#include <nx/vms/common/system_context.h>

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

QVariantMap SaasIssueEvent::details(
    common::SystemContext* context,
    Qn::ResourceInfoLevel detailLevel) const
{
    auto result = BasicEvent::details(context, detailLevel);
    fillAggregationDetailsForServer(result, context, serverId(), detailLevel);

    utils::insertLevel(result, nx::vms::event::Level::important);
    utils::insertIcon(result, nx::vms::rules::Icon::license);
    utils::insertClientAction(result, nx::vms::rules::ClientAction::licensesSettings);

    result[utils::kCaptionDetailName] = manifest().displayName();

    auto [reasonText, devices] = detailing(context);

    QStringList details{{reasonText}};
    static const QString kRow = "- %1";
    for (const auto& device: devices)
        details.push_back(kRow.arg(device));
    result[utils::kDescriptionDetailName] = details.join('\n');
    result[utils::kDetailingDetailName] = details;
    result[utils::kHtmlDetailsName] = QStringList{{
        reasonText,
        devices.join(common::html::kLineBreak)
    }};

    return result;
}

QString SaasIssueEvent::extendedCaption(common::SystemContext* context,
    Qn::ResourceInfoLevel detailLevel) const
{
    const auto resourceName = Strings::resource(context, serverId(), detailLevel);
    return tr("Services Issue on %1").arg(resourceName);
}

QString SaasIssueEvent::licenseMigrationReason() const
{
    using namespace nx::vms::api;

    switch (m_reason)
    {
        case EventReason::licenseMigrationFailed:
            return tr("Failed to migrate licenses.");

        case EventReason::licenseMigrationSkipped:
            return tr("Skipped import of licenses. Another migration attempt will be "
                "automatically scheduled for later.");

        default:
            NX_ASSERT(false, "Unexpected reason code");
            return {};
    }
}

bool SaasIssueEvent::isLicenseMigrationIssue(nx::vms::api::EventReason reason)
{
    using namespace nx::vms::api;

    switch (reason)
    {
        case EventReason::licenseMigrationFailed:
        case EventReason::licenseMigrationSkipped:
            return true;

        case EventReason::notEnoughLocalRecordingServices:
        case EventReason::notEnoughCloudRecordingServices:
        case EventReason::notEnoughIntegrationServices:
        case EventReason::none: //< Special case for unit tests.
            return false;

        default:
            NX_ASSERT(false, "Unhandled reason code");
            return false;
    }
}

bool SaasIssueEvent::isLicenseMigrationIssue() const
{
    return isLicenseMigrationIssue(m_reason);
}

QString SaasIssueEvent::reason(nx::vms::common::SystemContext* context) const
{
    return isLicenseMigrationIssue()
        ? licenseMigrationReason()
        : serviceDisabledReason(context);
}

std::pair<QString, QStringList> SaasIssueEvent::detailing(
    nx::vms::common::SystemContext* context) const
{
    QStringList result;
    if (isLicenseMigrationIssue())
    {
        for (const auto& key: m_licenseKeys)
        {
            if (const QnLicensePtr license = context->licensePool()->findLicense(key))
                result << QnLicense::displayText(license->type(), license->cameraCount());
            else
                result << key;
        }
    }
    else
    {
        const auto devices =
            context->resourcePool()->getResourcesByIds<QnVirtualCameraResource>(m_deviceIds);
        for (const auto& device: devices)
            result << Strings::resource(device, Qn::RI_WithUrl);
    }
    return std::make_pair(reason(context), result);
}

QString SaasIssueEvent::serviceDisabledReason(nx::vms::common::SystemContext* context) const
{
    using namespace nx::vms::api;

    if (m_reason == EventReason::none) //< Special case for unit tests.
        return {};

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
            {utils::kServerIdFieldName, {ResourceType::server}}}
    };
    return kDescriptor;
}

} // namespace nx::vms::rules
