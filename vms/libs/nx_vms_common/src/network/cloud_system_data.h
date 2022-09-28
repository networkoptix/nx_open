// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>

#include <nx/utils/uuid.h>
#include <nx/fusion/model_functions_fwd.h>

struct NX_VMS_COMMON_API QnCloudSystem
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
    bool system2faEnabled = false;
    std::string newestServerVersion;

    /** The VMS version reported by the last connected VMS server. */
    QString version;

    bool operator==(const QnCloudSystem &other) const;

    bool visuallyEqual(const QnCloudSystem& other) const;
};

#define QnCloudSystem_Fields (cloudId)(localId)(name)(ownerAccountEmail)(ownerFullName)(weight) \
    (lastLoginTimeUtcMs)(authKey)(online)(system2faEnabled)(version)
QN_FUSION_DECLARE_FUNCTIONS(QnCloudSystem, (metatype)(json), NX_VMS_COMMON_API)

typedef QList<QnCloudSystem> QnCloudSystemList;
Q_DECLARE_METATYPE(QnCloudSystemList);
