#ifndef QN_CONNECTION_INFO_H
#define QN_CONNECTION_INFO_H

#include <QtCore/QMetaType>
#include <QtCore/QSharedPointer>

#include <nx_ec/binary_serialization_helper.h>
#include <utils/common/software_version.h>

#include "compatibility.h"

struct QnConnectionInfo {
    QnConnectionInfo(): allowCameraChanges(true) {}

    QUrl ecUrl;
    QnSoftwareVersion version;
    QList<QnCompatibilityItem> compatibilityItems;
    int proxyPort;
    QString ecsGuid;
    QString publicIp;
    QString brand;
    bool allowCameraChanges;

    QN_DECLARE_STRUCT_SERIALIZATORS();
};

Q_DECLARE_METATYPE( QnConnectionInfo );

//TODO: #ak serialize version and compatibilityItems
QN_DEFINE_STRUCT_SERIALIZATORS (QnConnectionInfo, (version)(compatibilityItems)(proxyPort)(ecsGuid)(publicIp)(brand)(allowCameraChanges) )

// TODO: #Elric remove shared pointer?
typedef QSharedPointer<QnConnectionInfo> QnConnectionInfoPtr;

Q_DECLARE_METATYPE(QnConnectionInfoPtr);

#endif // QN_CONNECTION_INFO_H
