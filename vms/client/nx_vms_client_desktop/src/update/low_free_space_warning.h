#pragma once

#include <QtCore/QSet>

#include <utils/common/id.h>

struct QnLowFreeSpaceWarning
{
    QSet<QnUuid> failedPeers;
    bool ignore = false;
};
