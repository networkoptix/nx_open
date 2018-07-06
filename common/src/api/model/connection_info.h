#pragma once

#include <QtCore/QMetaType>
#include <QtCore/QSharedPointer>
#include <QtCore/QUrl>

#include <utils/common/software_version.h>
#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/uuid.h>
#include <nx/utils/url.h>
#include "compatibility_item.h"

/*
 * Please note: part of this structure up to effectiveUserName (included) CANNOT BE MODIFIED.
 * If changed, compatibility mode will stop working for clients 2.6 and older.
 */
struct QnConnectionInfo
{
    /*!
        \note considering that servers with no proto version support have version nx_ec::INITIAL_EC2_PROTO_VERSION
    */
    QnConnectionInfo();

    nx::utils::Url ecUrl;
    QnSoftwareVersion version;
    QList<QnCompatibilityItem> compatibilityItems;
    QString ecsGuid;    //< Id of remote server. Contains valid QnUuid.
    QString systemName;
    QString brand;
    QString box;
    bool allowSslConnections;
    int nxClusterProtoVersion; //!Transaction message bus protocol version (defined by \a nx_ec::EC2_PROTO_VERSION)
    bool ecDbReadOnly;
    QString effectiveUserName;

    /* --- 3.0 part goes further. It can be changed. ---- */

    bool newSystem;
    QString cloudHost;
    QString customization;
    QString cloudSystemId;
    QnUuid localSystemId;
    bool p2pMode;

    QnUuid serverId() const;

    /* Check if https protocol can be used. */
    nx::utils::Url effectiveUrl() const;
};

#define QnConnectionInfo_Fields (ecUrl)(version)(compatibilityItems)(ecsGuid)(systemName)(brand)\
    (box)(allowSslConnections)(nxClusterProtoVersion)(ecDbReadOnly)(effectiveUserName)(newSystem)\
    (cloudHost)(customization)(cloudSystemId)(localSystemId)(p2pMode)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (QnCompatibilityItem)(QnConnectionInfo),
    (ubjson)(metatype)(xml)(json)(csv_record)
)

