#include "http_server_htdigest_authentication_provider.h"

namespace nx::network::http::server {

using namespace nx::network::http;

HtDigestAuthenticationProvider::HtDigestAuthenticationProvider(const std::string& filePath)
{
    std::ifstream file(filePath);
    if (file.is_open())
    {
        load(file);
        file.close();
    }
}

HtDigestAuthenticationProvider::HtDigestAuthenticationProvider(std::istream& input)
{
    load(input);
}

void HtDigestAuthenticationProvider::getPasswordByUserName(
    const nx::String& userName,
    LookupResultHandler completionHandler)
{
    PasswordLookupResult result;

    {
        QnMutexLocker lock(&m_mutex);
        auto it = m_credentials.find(userName);

        if (it == m_credentials.end())
            result = Ha1LookupResultBuilder::build(PasswordLookupResult::Code::notFound);
        else
            result = Ha1LookupResultBuilder::build(it->second);
    }

    completionHandler(std::move(result));
}

void HtDigestAuthenticationProvider::load(std::istream& input)
{
    std::string line;
    while (std::getline(input, line))
    {
        // Each line is expected to be of the form:
        // someuser:realm:c30b86d219198b1ada5b63fbfa0818e3
        // where userName == "someuser", password == "c30b86d219198b1ada5b63fbfa0818e3"
        // and "realm" is the user's role that we ignore.

        std::size_t colon = line.find(':');
        if (colon == std::string::npos)
            continue;

        std::string userName = line.substr(0, colon);

        colon = line.find(':', colon + 1);
        if (colon == std::string::npos || colon >= line.size() - 1)
            continue;

        std::string password = line.substr(colon + 1);

        {
            QnMutexLocker lock(&m_mutex);
            m_credentials.emplace(QByteArray(userName.c_str()), QByteArray(password.c_str()));
        }
    }
}

} // namespace nx::network::http::server