#include "system_health.h"

bool QnSystemHealth::isMessageVisible( MessageType message ) {
    return message != QnSystemHealth::ArchiveFastScanFinished;
}
