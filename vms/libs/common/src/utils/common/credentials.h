#pragma once

#include <QtCore/QString>
#include <QtNetwork/QAuthenticator>

#include <nx/fusion/model_functions_fwd.h>

#include <nx/utils/url.h>

namespace nx::vms::common {

struct Credentials
{
    QString user;
    QString password;

private:
    Q_GADGET
    Q_PROPERTY(QString user MEMBER user CONSTANT)
    Q_PROPERTY(QString password MEMBER password CONSTANT)

public:
    Credentials() = default;
    Credentials(const QString& user, const QString& password);
    Credentials(const QAuthenticator& authenticator);
    Credentials(const nx::utils::Url& url);

    QAuthenticator toAuthenticator() const;

    /** Check both user and password are empty. */
    Q_INVOKABLE bool isEmpty() const;

    /** Check both user and password are filled. */
    Q_INVOKABLE bool isValid() const;
};

#define Credentials_Fields (user)(password)
QN_FUSION_DECLARE_FUNCTIONS(Credentials, (eq)(json))

} // namespace nx::vms::common

Q_DECLARE_METATYPE(nx::vms::common::Credentials)
