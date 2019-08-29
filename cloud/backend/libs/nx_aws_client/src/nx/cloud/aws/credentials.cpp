#include "credentials.h"

namespace nx::cloud::aws {

Credentials::Credentials(
    const QString& username,
    const network::http::AuthToken& authToken,
    const QString& sessionToken)
    :
    base_type(username, authToken),
    sessionToken(sessionToken)
{
}

} // namespace nx::cloud::aws
