#ifndef QN_CONNECTION_INFO_H
#define QN_CONNECTION_INFO_H

#include <QtCore/QMetaType>
#include <QtCore/QSharedPointer>

#include <utils/common/software_version.h>

#include "compatibility.h"

typedef QnSoftwareVersion SoftwareVersionType;

struct QnConnectionInfoData {
    QnConnectionInfoData(): proxyPort(0) {}

    QUrl ecUrl;
    SoftwareVersionType version;
    QList<QnCompatibilityItem> compatibilityItems;
    int proxyPort;
    QString ecsGuid;
    QString publicIp;
    QString brand;
};

//QN_DEFINE_STRUCT_SERIALIZATORS (QnConnectionInfoData, (ecUrl)(version)(compatibilityItems)(proxyPort)(ecsGuid)(publicIp)(brand))
QN_FUSION_DECLARE_FUNCTIONS(QnConnectionInfoData, (binary))

typedef QnConnectionInfoData QnConnectionInfo;
Q_DECLARE_METATYPE(QnConnectionInfo);


// TODO: #Elric remove shared pointer?
typedef QSharedPointer<QnConnectionInfo> QnConnectionInfoPtr;

Q_DECLARE_METATYPE(QnConnectionInfoPtr);

#endif // QN_CONNECTION_INFO_H
