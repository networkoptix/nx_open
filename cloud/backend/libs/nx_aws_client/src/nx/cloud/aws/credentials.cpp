#include "credentials.h"

namespace nx::cloud::aws {

Credentials::Credentials(
    const QString& userName,
    const network::http::AuthToken& authToken)
    :
    base_type(userName, authToken)
{
}

Credentials::Credentials(
    const QString& userName,
    const network::http::AuthToken& authToken,
    const QString& sessionToken)
    :
    base_type(userName, authToken),
    sessionToken(sessionToken)
{
}

} // namespace nx::cloud::aws