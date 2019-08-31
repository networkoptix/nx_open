#include "credentials.h"

namespace nx::cloud::aws {

Credentials::Credentials(nx::network::http::Credentials credentials):
    base_type(std::move(credentials))
{
}

Credentials::Credentials(
    const QString& username,
    const network::http::AuthToken& authToken,
    const nx::String& sessionToken)
    :
    base_type(username, authToken),
    sessionToken(sessionToken)
{
}

} // namespace nx::cloud::aws
