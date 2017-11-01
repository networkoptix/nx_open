#pragma once

#include <QtCore/QVariantMap>

#include <nx/utils/uuid.h>
#include <nx/utils/url.h>
#include <nx/network/socket_common.h>

struct QnLoginSession
{
    QnUuid id;
    QString systemName;
    nx::utils::Url url;

    QnLoginSession();

    QVariantMap toVariant() const;
    static QnLoginSession fromVariant(const QVariantMap &variant);
};
