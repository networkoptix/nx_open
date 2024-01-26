// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "storage_issue_event.h"

#include "../group.h"
#include "../utils/event_details.h"
#include "../utils/string_helper.h"
#include "../utils/type.h"

namespace nx::vms::rules {

QVariantMap StorageIssueEvent::details(common::SystemContext* context) const
{
    auto result = BasicEvent::details(context);

    utils::insertIfNotEmpty(result, utils::kExtendedCaptionDetailName, extendedCaption(context));
    utils::insertIfNotEmpty(result, utils::kReasonDetailName, reason(context));
    utils::insertIfNotEmpty(result, utils::kDetailingDetailName, reason(context));
    result.insert(utils::kEmailTemplatePathDetailName, manifest().emailTemplatePath);
    utils::insertLevel(result, nx::vms::event::Level::critical);
    utils::insertIcon(result, nx::vms::rules::Icon::storage);
    utils::insertClientAction(result, nx::vms::rules::ClientAction::serverSettings);

    return result;
}

StorageIssueEvent::StorageIssueEvent(
    std::chrono::microseconds timestamp,
    QnUuid serverId,
    nx::vms::api::EventReason reason,
    const QString& reasonText)
    :
    BasicEvent(timestamp),
    m_serverId(serverId),
    m_reason(reason),
    m_reasonText(reasonText)
{
}

QString StorageIssueEvent::resourceKey() const
{
    return m_serverId.toSimpleString();
}

QString StorageIssueEvent::extendedCaption(common::SystemContext* context) const
{
    const auto resourceName = utils::StringHelper(context).resource(serverId(), Qn::RI_WithUrl);
    return tr("Storage Issue at %1").arg(resourceName);
}

QString StorageIssueEvent::uniqueName() const
{
    return (m_reason == nx::vms::api::EventReason::backupFailedSourceFileError)
        ? utils::makeName(BasicEvent::uniqueName(), QString::number((int) m_reason))
        : utils::makeName(BasicEvent::uniqueName(), QString::number((int) m_reason), m_reasonText);
}

QString StorageIssueEvent::reason(common::SystemContext* /*context*/) const
{
    using nx::vms::api::EventReason;
    const QString storageUrl = m_reasonText;

    switch (m_reason)
    {
        case EventReason::storageIoError:
            return tr("I/O error has occurred at %1.").arg(storageUrl);

        case EventReason::storageTooSlow:
            return tr("Not enough HDD/SSD speed for recording to %1.").arg(storageUrl);

        case EventReason::storageFull:
            return tr("HDD/SSD disk \"%1\" is full. Disk contains too much data"
                " that is not managed by VMS.")
                .arg(storageUrl);

        case EventReason::systemStorageFull:
            return tr("System disk \"%1\" is almost full.").arg(storageUrl);

        case EventReason::metadataStorageOffline:
            return tr("Analytics storage \"%1\" is offline.").arg(storageUrl);

        case EventReason::metadataStorageFull:
            return tr("Analytics storage \"%1\" is almost full.").arg(storageUrl);

        case EventReason::metadataStoragePermissionDenied:
            return tr(
                "Analytics storage \"%1\" database error: Insufficient permissions on the mount "
                "point.").arg(storageUrl);

        case EventReason::encryptionFailed:
            return tr("Cannot initialize AES encryption while recording is enabled on the media "
                "archive. Data is written unencrypted.");

        case EventReason::raidStorageError:
            return tr("RAID error: %1.").arg(m_reasonText);

        case EventReason::backupFailedSourceFileError:
            return tr("Archive backup failed. Failed to backup file %1.").arg(m_reasonText);

        default:
            NX_ASSERT(0, "Unexpected reason");
            return {};
    }
}

const ItemDescriptor& StorageIssueEvent::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = utils::type<StorageIssueEvent>(),
        .groupId = kServerIssueEventGroup,
        .displayName = tr("Storage Issue"),
        .permissions = {.globalPermission = GlobalPermission::powerUser},
        .emailTemplatePath = ":/email_templates/storage_failure.mustache"
    };
    return kDescriptor;
}

} // namespace nx::vms::rules
