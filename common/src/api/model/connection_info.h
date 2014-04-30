#ifndef QN_CONNECTION_INFO_H
#define QN_CONNECTION_INFO_H

#include <QtCore/QMetaType>
#include <QtCore/QSharedPointer>

#include <utils/common/software_version.h>
#include <utils/common/model_functions_fwd.h>

#include "compatibility_item.h"

typedef QnSoftwareVersion SoftwareVersionType; // TODO: #Elric remove?

struct QnConnectionInfo {
    QnConnectionInfo(): proxyPort(0) {}

    QUrl ecUrl;
    SoftwareVersionType version;
    QList<QnCompatibilityItem> compatibilityItems;
    int proxyPort;
    QString ecsGuid;
    QString publicIp;
    QString brand;
};

QN_FUSION_DECLARE_FUNCTIONS(QnCompatibilityItem, (metatype)(json)(binary))
QN_FUSION_DECLARE_FUNCTIONS(QnConnectionInfo, (metatype)(json)(binary))

// TODO: #Elric remove shared pointer?
typedef QSharedPointer<QnConnectionInfo> QnConnectionInfoPtr;
Q_DECLARE_METATYPE(QnConnectionInfoPtr);

#endif // QN_CONNECTION_INFO_H
