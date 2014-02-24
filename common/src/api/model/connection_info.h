#ifndef QN_CONNECTION_INFO_H
#define QN_CONNECTION_INFO_H

#include <QtCore/QMetaType>
#include <QtCore/QSharedPointer>

#include <nx_ec/binary_serialization_helper.h>
#include <utils/common/software_version.h>

#include "compatibility.h"

struct QnConnectionInfo {
    QnConnectionInfo(): proxyPort(0) {}

    QUrl ecUrl;
    QnSoftwareVersion version;
    QList<QnCompatibilityItem> compatibilityItems;
    int proxyPort;
    QString ecsGuid;
    QString publicIp;
    QString brand;

};

Q_DECLARE_METATYPE( QnConnectionInfo );

//TODO: #ak serialize version and compatibilityItems
QN_DEFINE_STRUCT_SERIALIZATORS (QnConnectionInfo, (version)(compatibilityItems)(proxyPort)(ecsGuid)(publicIp)(brand))

// TODO: #Elric remove shared pointer?
typedef QSharedPointer<QnConnectionInfo> QnConnectionInfoPtr;

Q_DECLARE_METATYPE(QnConnectionInfoPtr);

#endif // QN_CONNECTION_INFO_H
