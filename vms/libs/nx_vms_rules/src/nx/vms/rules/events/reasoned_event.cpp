// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "server_failure_event.h"

namespace nx::vms::rules {

ReasonedEvent::ReasonedEvent(
    std::chrono::microseconds timestamp,
    QnUuid serverId,
    EventReason reasonCode,
    const QString& reasonText)
    :
    base_type(timestamp),
    m_serverId(serverId),
    m_reasonCode(reasonCode),
    m_reasonText(reasonText)
{
}

QString ReasonedEvent::uniqueName() const
{
    switch (m_reasonCode)
    {
        case EventReason::storageIoError:
        case EventReason::storageTooSlow:
        case EventReason::storageFull:
        case EventReason::systemStorageFull:
        case EventReason::metadataStorageOffline:
        case EventReason::metadataStorageFull:
        case EventReason::metadataStoragePermissionDenied:
        case EventReason::raidStorageError:
        case EventReason::encryptionFailed:
            return makeName(
                BasicEvent::uniqueName(),
                m_serverId.toSimpleString(),
                QString::number(static_cast<int>(m_reasonCode)),
                m_reasonText);
        default:
            return makeName(
                BasicEvent::uniqueName(),
                m_serverId.toSimpleString(),
                QString::number(static_cast<int>(m_reasonCode)));
    }
}

} // namespace nx::vms::rules
