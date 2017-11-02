#include "system_health.h"

bool QnSystemHealth::isRemoteArchiveMessage(MessageType message)
{
    return message >= QnSystemHealth::RemoteArchiveSyncStarted
        && message <= QnSystemHealth::RemoteArchiveSyncError;
}

bool QnSystemHealth::isMessageVisible(MessageType message)
{
    switch (message)
    {
        case QnSystemHealth::ArchiveFastScanFinished:
        case QnSystemHealth::NoPrimaryTimeServer:
            return false;
    }

    return true;
}

bool QnSystemHealth::isMessageOptional(MessageType message)
{
    /* Hidden messages must not be disabled. */
    if (!isMessageVisible(message))
        return false;

    return message != QnSystemHealth::CloudPromo;
}

bool QnSystemHealth::isMessageLocked(MessageType message)
{
    switch (message)
    {
        case QnSystemHealth::CloudPromo:
        case QnSystemHealth::EmailIsEmpty:
        case QnSystemHealth::NoLicenses:
        case QnSystemHealth::SmtpIsNotSet:
        case QnSystemHealth::UsersEmailIsEmpty:
        case QnSystemHealth::SystemIsReadOnly:
        case QnSystemHealth::StoragesNotConfigured:
        case QnSystemHealth::StoragesAreFull:
            return true;
        default:
            break;
    }
    return false;
}
