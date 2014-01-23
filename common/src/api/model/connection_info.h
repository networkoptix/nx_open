#ifndef QN_CONNECTION_INFO_H
#define QN_CONNECTION_INFO_H

#include <QtCore/QSharedPointer>
#include <QtCore/QMetaType>

#include <utils/common/software_version.h>

#include "compatibility.h"

struct QnConnectionInfo {
    QnConnectionInfo(): allowCameraChanges(true) {}

    QnSoftwareVersion version;
    QList<QnCompatibilityItem> compatibilityItems;
    int proxyPort;
    QString ecsGuid;
    QString publicIp;
    QString brand;
    bool allowCameraChanges;
};

// TODO: #Elric remove shared pointer?
typedef QSharedPointer<QnConnectionInfo> QnConnectionInfoPtr;

Q_DECLARE_METATYPE(QnConnectionInfoPtr);

#endif // QN_CONNECTION_INFO_H
