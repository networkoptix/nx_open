#ifndef QN_CONNECTION_INFO_H
#define QN_CONNECTION_INFO_H

#ifndef QN_NO_QT
#include <QtCore/QMetaType>
#include <QtCore/QSharedPointer>
#endif

#ifndef QN_NO_BASE
#include <utils/common/software_version.h>
#include <utils/common/model_functions_fwd.h>

#include "compatibility_item.h"

typedef QnSoftwareVersion SoftwareVersionType; // TODO: #Elric remove?
#endif


struct QnConnectionInfo {
    QnConnectionInfo(): proxyPort(0) {}

    QUrl ecUrl;
    SoftwareVersionType version;
    QList<QnCompatibilityItem> compatibilityItems;
    int proxyPort;
    QString ecsGuid;
    QString publicIp;
    QString brand;
    QString systemName;
};

#define QnConnectionInfo_Fields (ecUrl)(version)(compatibilityItems)(proxyPort)(ecsGuid)(publicIp)(brand)(systemName)

#ifndef QN_NO_QT
QN_FUSION_DECLARE_FUNCTIONS(QnCompatibilityItem, (metatype)(json)(binary)(csv_record))
QN_FUSION_DECLARE_FUNCTIONS(QnConnectionInfo, (metatype)(json)(binary)(csv_record))

// TODO: #Elric remove shared pointer?
typedef QSharedPointer<QnConnectionInfo> QnConnectionInfoPtr;
Q_DECLARE_METATYPE(QnConnectionInfoPtr);
#endif

#endif // QN_CONNECTION_INFO_H
