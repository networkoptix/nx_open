#ifndef QN_CONNECTION_INFO_H
#define QN_CONNECTION_INFO_H

#include <QtCore/QMetaType>
#include <QtCore/QSharedPointer>

#include <utils/common/software_version.h>

#include "compatibility.h"

typedef QnSoftwareVersion SoftwareVersionType;

#include "connection_info_i.h"

typedef QnConnectionInfoData QnConnectionInfo;
Q_DECLARE_METATYPE(QnConnectionInfo);


// TODO: #Elric remove shared pointer?
typedef QSharedPointer<QnConnectionInfo> QnConnectionInfoPtr;

Q_DECLARE_METATYPE(QnConnectionInfoPtr);

#endif // QN_CONNECTION_INFO_H
