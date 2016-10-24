
#pragma once

#include <QtCore/QUrl>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/uuid.h>

class QSettings;

struct QnLocalConnectionData
{
    QnLocalConnectionData();

    QnLocalConnectionData(
        const QString& systemName,
        const QnUuid& localId,
        const QUrl& url,
        bool isStoredPassword);

    QString systemName;
    QnUuid localId;

    QUrl url;
    bool isStoredPassword;
};

#define QnLocalConnectionData_Fields (systemName)(localId)(url)(isStoredPassword)
QN_FUSION_DECLARE_FUNCTIONS(QnLocalConnectionData, (datastream)(metatype)(eq)(json))

typedef QList<QnLocalConnectionData> QnLocalConnectionDataList;
Q_DECLARE_METATYPE(QnLocalConnectionDataList)

struct QnWeightData
{
    QnUuid localId;
    qreal weight;
    qint64 lastConnectedUtcMs;
    bool realConnection;    //< Shows if it was real connection or just record for new system
};

#define QnWeightData_Fields (localId)(weight)(lastConnectedUtcMs)(realConnection)
QN_FUSION_DECLARE_FUNCTIONS(QnWeightData, (datastream)(metatype)(eq)(json))

typedef QList<QnWeightData> QnWeightDataList;
Q_DECLARE_METATYPE(QnWeightDataList)
