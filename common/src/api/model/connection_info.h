#ifndef QN_CONNECTION_INFO_H
#define QN_CONNECTION_INFO_H

#ifndef QN_NO_QT
#include <QtCore/QMetaType>
#include <QtCore/QSharedPointer>
#include <QtCore/QUrl>
#endif

#ifndef QN_NO_BASE
#include <utils/common/software_version.h>
#include <utils/common/model_functions_fwd.h>

#include "compatibility_item.h"

typedef QnSoftwareVersion SoftwareVersionType; // TODO: #Elric #ec2 remove
#endif


struct QnConnectionInfo {
    /*!
        \note considering that servers with no proto version support have version nx_ec::INITIAL_EC2_PROTO_VERSION
    */
    QnConnectionInfo();

    QUrl ecUrl;
    SoftwareVersionType version;
    QList<QnCompatibilityItem> compatibilityItems;
    QString systemName;
    QString ecsGuid;    //TODO: #GDM make QnUuid
    QString brand;
    QString box;
    bool allowSslConnections;
    //!Transaction message bus protocol version (defined by \a nx_ec::EC2_PROTO_VERSION)
    int nxClusterProtoVersion;
};

#define QnConnectionInfo_Fields (ecUrl)(version)(compatibilityItems)(ecsGuid)(systemName)(brand)(box)(allowSslConnections)(nxClusterProtoVersion)

#ifndef QN_NO_QT
QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (QnCompatibilityItem)(QnConnectionInfo), 
    (ubjson)(metatype)(xml)(json)(csv_record)
)
#endif

#endif // QN_CONNECTION_INFO_H
