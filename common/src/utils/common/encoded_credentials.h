#pragma once

#include <QtCore/QUrl>

#include <utils/crypt/encoded_string.h>

struct QnEncodedCredentials
{
    QString user;
    QnEncodedString password;

private:
    Q_GADGET
    Q_PROPERTY(QString user MEMBER user CONSTANT)
    Q_PROPERTY(QString password READ decodedPassword CONSTANT)

public:
    QnEncodedCredentials() = default;
    QnEncodedCredentials(const QString& user, const QString& password);
    explicit QnEncodedCredentials(const QUrl& url);

    QString decodedPassword() const;

    Q_INVOKABLE bool isEmpty() const;

    //! Check both user and password are filled.
    Q_INVOKABLE bool isValid() const;
};

#define QnEncodedCredentials_Fields (user)(password)
QN_FUSION_DECLARE_FUNCTIONS(QnEncodedCredentials, (metatype)(eq)(json))
