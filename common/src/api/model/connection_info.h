#pragma once

#ifndef QN_NO_QT
#include <QtCore/QMetaType>
#include <QtCore/QSharedPointer>
#include <QtCore/QUrl>
#endif

#ifndef QN_NO_BASE
#include <utils/common/software_version.h>
#include <nx/fusion/model_functions_fwd.h>

#include "compatibility_item.h"

#endif

struct QnConnectionInfo
{
    /*!
        \note considering that servers with no proto version support have version nx_ec::INITIAL_EC2_PROTO_VERSION
    */
    QnConnectionInfo();

    QUrl ecUrl;
    QnSoftwareVersion version;
    QList<QnCompatibilityItem> compatibilityItems;
    QString systemName;
    QString ecsGuid;    // Id of remote server. TODO: #GDM make QnUuid
    QString brand;
    QString box;
    bool allowSslConnections;
    //!Transaction message bus protocol version (defined by \a nx_ec::EC2_PROTO_VERSION)
    int nxClusterProtoVersion;
    bool ecDbReadOnly;
    bool newSystem;
    QString customization;
    QString effectiveUserName;
    QString cloudHost;
    QString cloudSystemId;
	QString localSystemId;

    /* Check if https protocol can be used. */
    QUrl effectiveUrl() const;
};

#define QnConnectionInfo_Fields (ecUrl)(version)(compatibilityItems)(ecsGuid)(systemName)(brand)\
    (box)(allowSslConnections)(nxClusterProtoVersion)(ecDbReadOnly)(effectiveUserName)(newSystem)\
    (cloudHost)(customization)(cloudSystemId)(localSystemId)

#ifndef QN_NO_QT
QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (QnCompatibilityItem)(QnConnectionInfo),
    (ubjson)(metatype)(xml)(json)(csv_record)
)
#endif
