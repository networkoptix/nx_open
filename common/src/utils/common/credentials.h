#pragma once
#include <QtCore>
#include <QtNetwork>
#include <utils/common/model_functions_fwd.h>

struct QnCredentials{

    QnCredentials() {};
    QnCredentials(const QString& usr, const QString& pwd) :
        user(usr),
        password(pwd) {};
    QnCredentials(const QAuthenticator& auth):
        user(auth.user()),
        password(auth.password()) {};

    QAuthenticator toAuthenticator() const;

    bool operator==(const QnCredentials& other) const;

    QString user;
    QString password;
};

inline uint qHash(const QnCredentials& creds)
{
    return qHash(creds.user) ^ qHash(creds.password);
}

inline uint qHash(const QAuthenticator& auth)
{
    return qHash(auth.user()) ^ qHash(auth.password());
}

#define QnCredentials_Fields (user)(password)
QN_FUSION_DECLARE_FUNCTIONS(QnCredentials, (json)(ubjson)(xml)(csv_record))
Q_DECLARE_METATYPE(QnCredentials)



