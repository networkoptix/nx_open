// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/reflect/instrument.h>
#include <nx/utils/uuid.h>

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

    /** The VMS version reported by the last connected VMS server. */
    QString version;

    QString organizationId;

    bool operator==(const QnCloudSystem &other) const;

    bool visuallyEqual(const QnCloudSystem& other) const;
};

NX_REFLECTION_INSTRUMENT(QnCloudSystem,
    (cloudId)(localId)(name)(ownerAccountEmail)(ownerFullName)(weight)(lastLoginTimeUtcMs)
    (authKey)(online)(system2faEnabled)(version)(organizationId))

typedef QList<QnCloudSystem> QnCloudSystemList;
