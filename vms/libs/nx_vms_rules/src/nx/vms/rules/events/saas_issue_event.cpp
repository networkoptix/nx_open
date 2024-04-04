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
#include "../utils/event_details.h"
#include "../utils/field.h"
#include "../utils/string_helper.h"
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
}

QString SaasIssueEvent::resourceKey() const
{
    return m_serverId.toSimpleString();
}

QVariantMap SaasIssueEvent::details(common::SystemContext* context) const
{
    auto result = BasicEvent::details(context);

    utils::insertIfNotEmpty(result, utils::kExtendedCaptionDetailName, extendedCaption(context));
    utils::insertIfNotEmpty(result, utils::kReasonDetailName, reason(context));
    result.insert(utils::kEmailTemplatePathDetailName, manifest().emailTemplatePath);
    utils::insertLevel(result, nx::vms::event::Level::important);
    utils::insertIcon(result, nx::vms::rules::Icon::alert);
    utils::insertClientAction(result, nx::vms::rules::ClientAction::licensesSettings);

    return result;
}

QString SaasIssueEvent::extendedCaption(common::SystemContext* context) const
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
        .displayName = tr("Services issue"),
        .permissions = {
            .resourcePermissions = {{"deviceIds", Qn::ViewContentPermission}}
        },
        .emailTemplatePath = ":/email_templates/saas_issue.mustache"
    };
    return kDescriptor;
}

QStringList SaasIssueEvent::licenseMigrationReason(nx::vms::common::SystemContext* context) const
{
    QStringList result;
    result << nx::vms::event::StringsHelper::licenseMigrationReason(m_reason);
    result << nx::vms::event::StringsHelper::licenseMigrationDetails(m_licenseKeys, context);
    return result;
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

QStringList SaasIssueEvent::reason(nx::vms::common::SystemContext* context) const
{
    return isLicenseMigrationIssue()
        ? licenseMigrationReason(context)
        : serviceDisabledReason(context);
}

QStringList SaasIssueEvent::serviceDisabledReason(nx::vms::common::SystemContext* context) const
{
    using namespace nx::vms::api;

    const auto devices =
        context->resourcePool()->getResourcesByIds<QnVirtualCameraResource>(m_deviceIds);
    if (!NX_ASSERT(!devices.isEmpty(), "At least one device should be affected by this event"))
        return {};

    QStringList result;

    switch (m_reason)
    {
        case EventReason::notEnoughLocalRecordingServices:
            result.push_back(tr("Recording on %n channels was stopped due to service overuse.",
                "", devices.count()));
            break;

        case EventReason::notEnoughCloudRecordingServices:
            result.push_back(tr(
                "Cloud storage backup on %n channels was stopped due to service overuse.",
                    "", devices.count()));
            break;

        case EventReason::notEnoughIntegrationServices:
            result.push_back(tr(
                "Paid integration service usage on %n channels was stopped due to service overuse.",
                    "", devices.count()));
            break;

        default:
            NX_ASSERT(false, "Unexpected event reason");
            return {};
    }

    return result;
}

} // namespace nx::vms::rules
