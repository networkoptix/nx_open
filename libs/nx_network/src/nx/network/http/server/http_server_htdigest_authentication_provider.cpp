#include "http_server_htdigest_authentication_provider.h"

namespace nx::network::http::server {

HtdigestAuthenticationProvider::HtdigestAuthenticationProvider(const std::string& filePath)
{
    if (filePath.empty())
        return;

    std::ifstream file(filePath);
    if (!file.is_open())
    {
        NX_ERROR(this, lm("Failed to open htdigest file: %1").arg(filePath));
        return;
    }

    NX_INFO(this, lm("Opening htdigest file to load credentials: %1").arg(filePath));

    load(file);
    file.close();
}

HtdigestAuthenticationProvider::HtdigestAuthenticationProvider(std::istream& input)
{
    load(input);
}

void HtdigestAuthenticationProvider::getPasswordByUserName(
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

void HtdigestAuthenticationProvider::load(std::istream& input)
{
    std::string line;
    int lineNumber = 1;
    for (; std::getline(input, line); ++lineNumber)
    {
        // Each line is expected to be of the form:
        // someuser:realm:c30b86d219198b1ada5b63fbfa0818e3
        // where userName == "someuser", password == "c30b86d219198b1ada5b63fbfa0818e3"
        // and "realm" is the user's role that we ignore.

        std::size_t colon = line.find(':');
        if (colon == std::string::npos)
        {
            NX_ERROR(this, lm("Malformed line: missing ':' on line: %1").arg(lineNumber));
            continue;
        }

        std::string userName = line.substr(0, colon);

        colon = line.find(':', colon + 1);
        if (colon == std::string::npos || colon >= line.size() - 1)
        {
            NX_ERROR(this, lm("Malformed line: missing second ':' on line: %1").arg(lineNumber));
            continue;
        }

        std::string digest = line.substr(colon + 1);

        {
            QnMutexLocker lock(&m_mutex);
            m_credentials.emplace(userName.c_str(), digest.c_str());
        }
    }

    if (lineNumber == 1)
        NX_WARNING(this, lm("Empty htdigest file provided, authentication will fail"));
}

} // namespace nx::network::http::server