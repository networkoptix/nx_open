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
            result = PlainTextPasswordLookupResultBuilder::build(it->second);
        }
    }

    completionHandler(std::move(result));
}

void PlainTextCredentialsProvider::addCredentials(
    const nx::String& userName,
    const nx::String& password)
{
    QnMutexLocker lock(&m_mutex);
    m_credentials.emplace(userName, password);
}

} // namespace server
} // namespace nx
} // namespace network
} // namespace http
