#include "login_session.h"

#include <QtCore/QUuid>

QnLoginSession::QnLoginSession() :
    id(QUuid::createUuid().toString()),
    port(-1)
{
}

QUrl QnLoginSession::url() const {
    QUrl url;
    url.setScheme(lit("http"));
    url.setHost(address);
    url.setPort(port);
    url.setUserName(user);
    url.setPassword(password);
    return url;
}

QVariantMap QnLoginSession::toVariant() const {
    QVariantMap variant;
    variant[lit("sessionId")] = id;
    variant[lit("systemName")] = systemName;
    variant[lit("address")] = address;
    variant[lit("port")] = port;
    variant[lit("user")] = user;
    variant[lit("password")] = password;
    return variant;
}

QnLoginSession QnLoginSession::fromVariant(const QVariantMap &variant) {
    QnLoginSession session;
    session.id = variant[lit("sessionId")].toString();
    session.systemName = variant[lit("systemName")].toString();
    session.address = variant[lit("address")].toString();
    session.port = variant.value(lit("port"), -1).toInt();
    session.user = variant[lit("user")].toString();
    session.password = variant[lit("password")].toString();
    return session;
}
