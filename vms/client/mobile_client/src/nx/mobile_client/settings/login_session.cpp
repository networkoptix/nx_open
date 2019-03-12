#include "login_session.h"

#include <nx/network/url/url_builder.h>
#include <nx/network/http/http_types.h>

QnLoginSession::QnLoginSession():
    id(QnUuid::createUuid().toString())
{
}

QVariantMap QnLoginSession::toVariant() const
{
    QVariantMap variant;
    variant[lit("sessionId")] = id.toString();
    variant[lit("systemName")] = systemName;
    variant[lit("address")] = url.host();
    variant[lit("port")] = url.port();
    variant[lit("user")] = url.userName();
    variant[lit("password")] = url.password();
    return variant;
}

QnLoginSession QnLoginSession::fromVariant(const QVariantMap& variant)
{
    QnLoginSession session;
    session.id = QnUuid::fromStringSafe(variant[lit("sessionId")].toString());
    session.systemName = variant[lit("systemName")].toString();
    session.url = nx::network::url::Builder()
        .setScheme(nx::network::http::kSecureUrlSchemeName)
        .setHost(variant[lit("address")].toString())
        .setPort(variant.value(lit("port"), -1).toInt())
        .setUserName(variant[lit("user")].toString())
        .setPassword(variant[lit("password")].toString());
    return session;
}
