#ifndef COMMON_CONNECTINFO_H
#define COMMON_CONNECTINFO_H

#include <QtCore/QSharedPointer>
#include <QtCore/QMetaType>

#include "compatibility.h"

struct QnConnectInfo
{
    QString version;
    QList<QnCompatibilityItem> compatibilityItems;
    int proxyPort;
    QString ecsGuid;
    QString publicIp;
};

// TODO: #Elric remove shared pointer.
typedef QSharedPointer<QnConnectInfo> QnConnectInfoPtr;

Q_DECLARE_TYPEINFO(QnConnectInfoPtr, Q_MOVABLE_TYPE);
Q_DECLARE_METATYPE(QnConnectInfoPtr);

#endif // COMMON_CONNECTINFO_H
