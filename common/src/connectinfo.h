#ifndef COMMON_CONNECTINFO_H
#define COMMON_CONNECTINFO_H

#include <QSharedPointer>

#include "compatibility.h"

struct QnConnectInfo
{
    QString version;
    QList<QnCompatibilityItem> compatibilityItems;
    int proxyPort;
};

typedef QSharedPointer<QnConnectInfo> QnConnectInfoPtr;

#endif // COMMON_CONNECTINFO_H
