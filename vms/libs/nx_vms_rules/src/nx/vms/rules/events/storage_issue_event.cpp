// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "storage_issue_event.h"

#include "../group.h"
#include "../strings.h"
#include "../utils/event_details.h"
#include "../utils/field.h"
#include "../utils/type.h"

namespace nx::vms::rules {

QVariantMap StorageIssueEvent::details(
    common::SystemContext* context, const nx::vms::api::rules::PropertyMap& aggregatedInfo) const
{
    auto result = BasicEvent::details(context, aggregatedInfo);

    utils::insertIfNotEmpty(result, utils::kExtendedCaptionDetailName, extendedCaption(context));
    utils::insertIfNotEmpty(result, utils::kReasonDetailName, reason(context));
    utils::insertLevel(result, nx::vms::event::Level::critical);
    utils::insertIcon(result, nx::vms::rules::Icon::storage);
    utils::insertClientAction(result, nx::vms::rules::ClientAction::serverSettings);

    return result;
}

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

QString StorageIssueEvent::resourceKey() const
{
    return m_serverId.toSimpleString();
}

QString StorageIssueEvent::extendedCaption(common::SystemContext* context) const
{
    const auto resourceName = Strings::resource(context, serverId(), Qn::RI_WithUrl);
    return tr("Storage Issue at %1").arg(resourceName);
}

QString StorageIssueEvent::aggregationSubKey() const
{
    return (m_reason == nx::vms::api::EventReason::backupFailedSourceFileError)
        ? utils::makeKey(BasicEvent::aggregationSubKey(), QString::number((int) m_reason))
        : utils::makeKey(
              BasicEvent::aggregationSubKey(), QString::number((int) m_reason), m_reasonText);
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
        .displayName = NX_DYNAMIC_TRANSLATABLE(tr("Storage Issue")),
        .description = "Triggered when the server fails to write data "
            "to one or more storage devices.",
        .resources = {{utils::kServerIdFieldName, {ResourceType::server}}},
        .readPermissions = GlobalPermission::powerUser,
        .emailTemplateName = "timestamp_and_details.mustache"
    };
    return kDescriptor;
}

} // namespace nx::vms::rules
