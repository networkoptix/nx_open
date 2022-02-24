// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "auth_session.h"
#include <nx/utils/qnbytearrayref.h>

#include <nx/fusion/model_functions.h>

namespace {
    char DELIMITER = '$';
}


nx::String QnAuthSession::toString() const
{
    auto encoded = [](QByteArray value) {
        return value.replace(DELIMITER, char('_'));
    };

    QByteArray result;
    result.append(id.toByteArray());
    result.append(DELIMITER);
    result.append(encoded(userName.toUtf8()));
    result.append(DELIMITER);
    result.append(encoded(userHost.toUtf8()));
    result.append(DELIMITER);
    result.append(encoded(userAgent.toUtf8()));

    return result;
}

void QnAuthSession::fromString(const nx::String& data)
{
    QnByteArrayConstRef ref(data);
    QList<QnByteArrayConstRef> params = ref.split(DELIMITER);
    if (params.size() > 0)
        id = QnUuid(params[0]);
    if (params.size() > 1)
        userName = QString::fromUtf8(params[1]);
    if (params.size() > 2)
        userHost = QString::fromUtf8(params[2]);
    if (params.size() > 3)
        userAgent = QString::fromUtf8(params[3]);
}

void serialize_field(const QnAuthSession&authData, QVariant *target)
{
    serialize_field(authData.toString(), target);
}

void deserialize_field(const QVariant &value, QnAuthSession *target)
{
    nx::String tmp;
    deserialize_field(value, &tmp);
    target->fromString(tmp);
}

