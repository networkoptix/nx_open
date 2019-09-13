#pragma once

#include <nx/network/http/auth_tools.h>

namespace nx::cloud::aws {

class NX_AWS_CLIENT_API Credentials:
    public nx::network::http::Credentials
{
    using base_type = nx::network::http::Credentials;

public:
    nx::String sessionToken;

    Credentials() = default;
    Credentials(nx::network::http::Credentials);
    Credentials(
        const QString& username,
        const network::http::AuthToken& authToken,
        const nx::String& sessionToken = nx::String());
};

} // namespace nx::cloud::aws
