#pragma once

#include <nx/vms/client/core/common/utils/encoded_string.h>
#include <nx/utils/url.h>

struct QnEncodedCredentials
{
    QString user;
    nx::vms::client::core::EncodedString password;

private:
    Q_GADGET
    Q_PROPERTY(QString user MEMBER user CONSTANT)
    Q_PROPERTY(QString password READ decodedPassword CONSTANT)

public:
    QnEncodedCredentials() = default;
    QnEncodedCredentials(
        const QString& user,
        const QString& password,
        const QByteArray& key = QByteArray());

    explicit QnEncodedCredentials(const nx::utils::Url& url, const QByteArray& key = QByteArray());

    QString decodedPassword() const;

    Q_INVOKABLE bool isEmpty() const;

    //! Check both user and password are filled.
    Q_INVOKABLE bool isValid() const;
};

#define QnEncodedCredentials_Fields (user)(password)
QN_FUSION_DECLARE_FUNCTIONS(QnEncodedCredentials, (metatype)(eq)(json))
