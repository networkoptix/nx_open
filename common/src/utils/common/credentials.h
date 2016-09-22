#pragma once

#include <QtCore/QString>
#include <QtNetwork/QAuthenticator>

#include <nx/fusion/model_functions_fwd.h>

struct QnCredentials{

    QnCredentials() {};
    QnCredentials(const QString& usr, const QString& pwd) :
        user(usr),
        password(pwd) {};
    QnCredentials(const QAuthenticator& auth):
        user(auth.user()),
        password(auth.password()) {};

    QAuthenticator toAuthenticator() const;

    QString user;
    QString password;

    bool isNull() const;
    bool isEmpty() const;

    /**
     * Check both user and password are filled.
     * @param allowEmptyPassword    Disable password check
     */
    bool isValid(bool allowEmptyPassword = false) const;

    bool operator<(const QnCredentials& other) const;
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
QN_FUSION_DECLARE_FUNCTIONS(QnCredentials, (metatype)(eq)(json)(ubjson)(xml)(csv_record))
