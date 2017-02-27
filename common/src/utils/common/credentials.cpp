#include "credentials.h"

#include <nx/fusion/model_functions.h>

namespace nx {
namespace common {
namespace utils {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(Credentials, (eq)(json), Credentials_Fields)

Credentials::Credentials()
{
}

Credentials::Credentials(const QString& user, const QString& password):
    user(user),
    password(password)
{
}

Credentials::Credentials(const QAuthenticator& authenticator):
    user(authenticator.user()),
    password(authenticator.password())
{
}

QAuthenticator Credentials::toAuthenticator() const
{
    QAuthenticator authenticator;
    authenticator.setUser(user);
    authenticator.setPassword(password);
    return authenticator;
}

} // namespace utils
} // namespace common
} // namespace nx
