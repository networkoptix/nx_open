#include "system_health.h"

namespace QnSystemHealth {

bool isMessageVisible(MessageType message)
{
    switch (message)
    {
        case ArchiveFastScanFinished:
        case 4 /*NoPrimaryTimeServer*/:
            return false;
    }

    return true;
}

bool isMessageVisibleInSettings(MessageType message)
{
    /* Hidden messages must not be disabled. */
    if (!isMessageVisible(message))
        return false;

    switch (message)
    {
        case CloudPromo:

        // TODO: remove these in VMS-7724
        case RemoteArchiveSyncFinished:
        case RemoteArchiveSyncProgress:
        case RemoteArchiveSyncError:
            return false;
    }

    return true;
}

bool isMessageLocked(MessageType message)
{
    switch (message)
    {
        case CloudPromo:
        case EmailIsEmpty:
        case NoLicenses:
        case SmtpIsNotSet:
        case UsersEmailIsEmpty:
        case SystemIsReadOnly:
        case StoragesNotConfigured:
        case StoragesAreFull:
            return true;
        default:
            break;
    }
    return false;
}

QSet<MessageType> allVisibleMessageTypes()
{
    QSet<MessageType> result;
    for (int i = 0; i < Count; i++)
    {
        const auto messageType = static_cast<MessageType>(i);
        if (isMessageVisibleInSettings(messageType))
            result.insert(messageType);
    }
    return result;
}

QSet<MessageType> unpackVisibleInSettings(quint64 packed)
{
    QSet<MessageType> result;

    quint64 healthFlag = 1;

    for (auto messageType: QnSystemHealth::allVisibleMessageTypes())
    {
        if ((packed & healthFlag) == healthFlag)
            result.insert(messageType);

        healthFlag <<= 1;
    }
    return result;
}

quint64 packVisibleInSettings(quint64 base, QSet<MessageType> messageTypes)
{
    quint64 healthFlag = 1;
    for (auto messageType: QnSystemHealth::allVisibleMessageTypes())
    {
        if (messageTypes.contains(messageType))
            base |= healthFlag;
        else
            base &= ~healthFlag;
        healthFlag <<= 1;
    }

    return base;
}

} // namespace QnSystemHealth
