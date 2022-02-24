// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "http_server_plain_text_credentials_provider.h"

namespace nx::network::http::server {

void PlainTextCredentialsProvider::getPasswordByUserName(
    const std::string& userName,
    LookupResultHandler completionHandler)
{
    PasswordLookupResult result;

    {
        NX_MUTEX_LOCKER lock(&m_mutex);

        const auto it = m_credentials.find(userName);
        if (it == m_credentials.end())
        {
            result = PlainTextPasswordLookupResultBuilder::build(
                PasswordLookupResult::Code::notFound);
        }
        else
        {
            result = PasswordLookupResult{
                PasswordLookupResult::Code::ok,
                it->second.authToken};
        }
    }

    completionHandler(std::move(result));
}

void PlainTextCredentialsProvider::addCredentials(const Credentials& credentials)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    m_credentials.emplace(credentials.username, credentials);
}

} // namespace nx::network::http::server
