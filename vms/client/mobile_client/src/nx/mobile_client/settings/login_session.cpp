#include "login_session.h"

#include <nx/network/url/url_builder.h>

QnLoginSession::QnLoginSession():
    id(QnUuid::createUuid().toString())
{
}

QVariantMap QnLoginSession::toVariant() const
{
    QVariantMap variant;
    variant[QStringLiteral("sessionId")] = id.toString();
    variant[QStringLiteral("systemName")] = systemName;
    variant[QStringLiteral("address")] = url.host();
    variant[QStringLiteral("port")] = url.port();
    variant[QStringLiteral("user")] = url.userName();
    variant[QStringLiteral("password")] = url.password();
    return variant;
}

QnLoginSession QnLoginSession::fromVariant(const QVariantMap& variant)
{
    QnLoginSession session;
    session.id = QnUuid::fromStringSafe(variant[QStringLiteral("sessionId")].toString());
    session.systemName = variant[QStringLiteral("systemName")].toString();
    session.url = nx::network::url::Builder()
        .setScheme(QStringLiteral("http"))
        .setHost(variant[QStringLiteral("address")].toString())
        .setPort(variant.value(QStringLiteral("port"), -1).toInt())
        .setUserName(variant[QStringLiteral("user")].toString())
        .setPassword(variant[QStringLiteral("password")].toString());
    return session;
}
