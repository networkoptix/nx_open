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
