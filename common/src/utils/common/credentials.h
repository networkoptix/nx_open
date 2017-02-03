#pragma once

#include <QtCore/QString>
#include <QtCore/QUrl>
#include <QtNetwork/QAuthenticator>

#include <nx/fusion/model_functions_fwd.h>
#include <utils/crypt/encoded_string.h>

struct QnCredentials
{
private:
    Q_GADGET
    Q_PROPERTY(QString user MEMBER user CONSTANT)
    Q_PROPERTY(QString password READ decodedPassword CONSTANT)

public:
    QnCredentials() {}
    QnCredentials(const QString& user, const QString& password):
        user(user),
        password(password)
    {}
    QnCredentials(const QAuthenticator& auth):
        user(auth.user()),
        password(auth.password())
    {}

    explicit QnCredentials(const QUrl& url):
        user(url.userName()),
        password(url.password())
    {}

    QAuthenticator toAuthenticator() const;

    QString user;
    QnEncodedString password;

    QString decodedPassword() const;

    Q_INVOKABLE bool isEmpty() const;

    /**
     * Check both user and password are filled.
     * @param allowEmptyPassword    Disable password check
     */
    Q_INVOKABLE bool isValid(bool allowEmptyPassword = false) const;

    bool operator<(const QnCredentials& other) const;
};

inline uint qHash(const QnCredentials& creds)
{
    return qHash(creds.user) ^ qHash(creds.password.value());
}

inline uint qHash(const QAuthenticator& auth)
{
    return qHash(auth.user()) ^ qHash(auth.password());
}

#define QnCredentials_Fields (user)(password)
QN_FUSION_DECLARE_FUNCTIONS(QnCredentials, (metatype)(eq)(json))
