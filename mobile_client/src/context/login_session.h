#pragma once

#include <QtCore/QVariantMap>

struct QnLoginSession {
    QString id;
    QString systemName;
    QString address;
    int port;
    QString user;
    QString password;

    QnLoginSession();

    QUrl url() const;

    QVariantMap toVariant() const;
    static QnLoginSession fromVariant(const QVariantMap &variant);
};
