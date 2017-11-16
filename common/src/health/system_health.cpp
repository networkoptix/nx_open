#include "system_health.h"

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

bool QnSystemHealth::isMessageVisibleInSettings(MessageType message)
{
    /* Hidden messages must not be disabled. */
    if (!isMessageVisible(message))
        return false;

    switch (message)
    {
        case QnSystemHealth::CloudPromo:

        // TODO: remove these in VMS-7724
        case QnSystemHealth::RemoteArchiveSyncFinished:
        case QnSystemHealth::RemoteArchiveSyncProgress:
        case QnSystemHealth::RemoteArchiveSyncError:
            return false;
    }

    return true;
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
