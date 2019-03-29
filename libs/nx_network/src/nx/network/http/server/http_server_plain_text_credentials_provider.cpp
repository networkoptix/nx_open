#include "http_server_plain_text_credentials_provider.h"

namespace nx {
namespace network {
namespace http {
namespace server {

void PlainTextCredentialsProvider::getPasswordByUserName(
    const nx::String& userName,
    LookupResultHandler completionHandler)
{
    PasswordLookupResult result;

    {
        QnMutexLocker lock(&m_mutex);

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
    QnMutexLocker lock(&m_mutex);
    m_credentials.emplace(credentials.username.toUtf8(), credentials);
}

} // namespace server
} // namespace nx
} // namespace network
} // namespace http
