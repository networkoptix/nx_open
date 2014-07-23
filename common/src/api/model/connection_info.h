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
    QnConnectionInfo() {}

    QUrl ecUrl;
    SoftwareVersionType version;
    QList<QnCompatibilityItem> compatibilityItems;
    QString systemName;
    QString ecsGuid;
    QString brand;
    QString box;
};

#define QnConnectionInfo_Fields (ecUrl)(version)(compatibilityItems)(ecsGuid)(systemName)(brand)(box)

#ifndef QN_NO_QT
QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (QnCompatibilityItem)(QnConnectionInfo), 
    (ubjson)(metatype)(xml)(json)(binary)(csv_record)
)
#endif

#endif // QN_CONNECTION_INFO_H
