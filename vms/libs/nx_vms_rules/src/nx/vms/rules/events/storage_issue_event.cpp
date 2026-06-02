// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "storage_issue_event.h"

#include "../group.h"
#include "../strings.h"
#include "../utils/event_details.h"
#include "../utils/field.h"
#include "../utils/type.h"

namespace nx::vms::rules {

StorageIssueEvent::StorageIssueEvent(
    std::chrono::microseconds timestamp,
    nx::Uuid serverId,
    nx::vms::api::EventReason reason,
    const QString& reasonText)
    :
    BasicEvent(timestamp),
    m_serverId(serverId),
    m_reason(reason),
    m_reasonText(reasonText)
{
}

QString StorageIssueEvent::aggregationKey() const
{
    return utils::makeKey(m_serverId.toSimpleString(), QString::number((int) m_reason));
}

QVariantMap StorageIssueEvent::details(
    common::SystemContext* context,
    Qn::ResourceInfoLevel detailLevel) const
{
    auto result = BasicEvent::details(context, detailLevel);
    fillAggregationDetailsForServer(result, context, serverId(), detailLevel);

    utils::insertLevel(result, nx::vms::event::Level::critical);
    utils::insertIcon(result, nx::vms::rules::Icon::storage);
    utils::insertClientAction(result, nx::vms::rules::ClientAction::serverSettings);

    result[utils::kCaptionDetailName] = manifest().displayName();

    const QString detailing = extendedReasonText();
    result[utils::kDescriptionDetailName] = detailing;
    result[utils::kDetailingDetailName] = detailing;
    result[utils::kHtmlDetailsName] = detailing;

    return result;
}

QString StorageIssueEvent::extendedCaption(common::SystemContext* context,
    Qn::ResourceInfoLevel detailLevel) const
{
    using nx::vms::api::EventReason;
    const auto resourceName = Strings::resource(context, serverId(), detailLevel);

    switch (m_reason)
    {
        case EventReason::storageIoError:
            return tr("Storage I/O Error at %1").arg(resourceName);

        case EventReason::storageTooSlow:
            return tr("Storage Too Slow at %1").arg(resourceName);

        case EventReason::storageFull:
            return tr("Storage Disk Full at %1").arg(resourceName);

        case EventReason::systemStorageFull:
            return tr("System Disk Almost Full at %1").arg(resourceName);

        case EventReason::metadataStorageOffline:
            return tr("Analytics Storage Offline at %1").arg(resourceName);

        case EventReason::metadataStorageFull:
            return tr("Analytics Storage Almost Full at %1").arg(resourceName);

        case EventReason::metadataStoragePermissionDenied:
            return tr("Analytics Storage Permission Error at %1").arg(resourceName);

        case EventReason::encryptionFailed:
            return tr("Storage Encryption Failed at %1").arg(resourceName);

        case EventReason::raidStorageError:
            return tr("RAID Storage Error at %1").arg(resourceName);

        case EventReason::backupFailedSourceFileError:
            return tr("Archive Backup Failed at %1").arg(resourceName);

        case EventReason::none: //< Special case for unit tests.
            return {};

        default:
            NX_ASSERT(0, "Unexpected reason: %1", m_reason);
            return {};
    }
}

QString StorageIssueEvent::extendedReasonText() const
{
    using nx::vms::api::EventReason;
    const QString storageUrl = m_reasonText;

    switch (m_reason)
    {
        case EventReason::storageIoError:
            return tr("I/O error has occurred at %1.").arg(storageUrl);

        case EventReason::storageTooSlow:
            return tr("Not enough HDD/SSD/Network speed for recording to %1.").arg(storageUrl);

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

        case EventReason::none: //< Special case for unit tests.
            return {};

        default:
            NX_ASSERT(0, "Unexpected reason: %1", m_reason);
            return {};
    }
}

const ItemDescriptor& StorageIssueEvent::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = utils::type<StorageIssueEvent>(),
        .groupId = kServerIssueEventGroup,
        .displayName = NX_DYNAMIC_TRANSLATABLE(tr("Storage Issue")),
        .description = "Triggered when the server fails to write data "
            "to one or more storage devices.",
        .resources = {{utils::kServerIdFieldName, {ResourceType::server}}},
        .readPermissions = GlobalPermission::powerUser
    };
    return kDescriptor;
}

} // namespace nx::vms::rules
