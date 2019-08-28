#pragma once

#include <nx/network/http/auth_tools.h>

namespace nx::cloud::aws {

class NX_AWS_CLIENT_API Credentials:
    public nx::network::http::Credentials
{
    using base_type = nx::network::http::Credentials;
public:
    QString sessionToken;

    Credentials() = default;
    Credentials(
        const QString& accessKeyId,
        const network::http::AuthToken& secretAccessKey);
    Credentials(
        const QString& accessKeyId,
        const network::http::AuthToken& secretAccessKey,
        const QString& sessionToken);
};

} // namespace nx::cloud::aws