#pragma once

#include <QtCore/QString>
#include <QtNetwork/QAuthenticator>

#include <nx/fusion/model_functions_fwd.h>

namespace nx {
namespace common {
namespace utils {

struct Credentials
{
    QString user;
    QString password;

    Credentials();
    Credentials(const QString& user, const QString& password);
    Credentials(const QAuthenticator& authenticator);

    QAuthenticator toAuthenticator() const;
};

#define Credentials_Fields (user)(password)
QN_FUSION_DECLARE_FUNCTIONS(Credentials, (eq)(json))

} // namespace utils
} // namespace common
} // namespace nx

Q_DECLARE_METATYPE(nx::common::utils::Credentials)
