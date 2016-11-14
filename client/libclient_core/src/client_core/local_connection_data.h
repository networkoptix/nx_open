#pragma once

#include <QtCore/QUrl>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/uuid.h>

#include <utils/crypt/encoded_string.h>

class QSettings;

struct QnLocalConnectionData
{
    QnLocalConnectionData();

    QnLocalConnectionData(
        const QString& systemName,
        const QnUuid& localId,
        const QUrl& url);

    QUrl urlWithPassword() const;

    bool isStoredPassword() const;

    QString systemName;
    QnUuid localId;
    QUrl url;
    QnEncodedString password;
};

#define QnLocalConnectionData_Fields (systemName)(localId)(url)(password)
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

namespace helpers {

QnLocalConnectionData storeLocalSystemConnection(
    const QString& systemName,
    const QnUuid& localSystemId,
    const QUrl& url);

}
