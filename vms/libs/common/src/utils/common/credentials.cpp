#include "credentials.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::common {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(Credentials, (eq)(json), Credentials_Fields)

Credentials::Credentials(const QString& user, const QString& password):
    user(user),
    password(password)
{
}

Credentials::Credentials(const QAuthenticator& authenticator):
    Credentials(authenticator.user(), authenticator.password())
{
}

Credentials::Credentials(const nx::utils::Url& url):
    Credentials(url.userName(), url.password())
{
}

QAuthenticator Credentials::toAuthenticator() const
{
    QAuthenticator authenticator;
    authenticator.setUser(user);
    authenticator.setPassword(password);
    return authenticator;
}

bool Credentials::isEmpty() const
{
    return user.isEmpty() && password.isEmpty();
}

bool Credentials::isValid() const
{
    return !user.isEmpty() && !password.isEmpty();
}

} // namespace nx::vms::common
