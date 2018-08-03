#pragma once

#include <QtCore/QString>

#include <nx/utils/uuid.h>
#include <nx/fusion/model_functions_fwd.h>

struct QnCloudSystem
{
    QString cloudId;
    QnUuid localId;
    QString name;
    QString ownerAccountEmail;
    QString ownerFullName;
    std::string authKey;
    qreal weight;
    qint64 lastLoginTimeUtcMs;
    bool online = false;

    bool operator==(const QnCloudSystem &other) const;

    bool visuallyEqual(const QnCloudSystem& other) const;
};

#define QnCloudSystem_Fields (cloudId)(localId)(name)(ownerAccountEmail)(ownerFullName)(weight) \
    (lastLoginTimeUtcMs)(authKey)
QN_FUSION_DECLARE_FUNCTIONS(QnCloudSystem, (json)(metatype)(json))

typedef QList<QnCloudSystem> QnCloudSystemList;
Q_DECLARE_METATYPE(QnCloudSystemList);
