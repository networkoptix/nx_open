#include "system_health.h"

bool QnSystemHealth::isMessageVisible(MessageType message)
{
    return message != QnSystemHealth::ArchiveFastScanFinished;
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
        case QnSystemHealth::NoPrimaryTimeServer:
        case QnSystemHealth::SystemIsReadOnly:
        case QnSystemHealth::StoragesNotConfigured:
        case QnSystemHealth::StoragesAreFull:
            return true;
        default:
            break;
    }
    return false;
}
