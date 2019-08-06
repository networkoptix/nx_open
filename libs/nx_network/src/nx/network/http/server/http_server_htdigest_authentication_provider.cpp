#include "http_server_htdigest_authentication_provider.h"

#include <string>
#include <vector>

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/predicate.hpp>

#include <nx/utils/system_error.h>

namespace nx::network::http::server {

HtdigestAuthenticationProvider::HtdigestAuthenticationProvider(const std::string& filePath)
{
    std::ifstream file(filePath);
    if (!file.is_open())
    {
        NX_ERROR(this, lm("Failed to open htdigest file: %1. Reason: %2. Authentication will fail.")
            .arg(filePath).arg(SystemError::getLastOSErrorText()));
        return;
    }

    NX_INFO(this, lm("Loading htdigest credentials from file: %1").arg(filePath));

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
    for (int lineNumber = 1; std::getline(input, line); ++lineNumber)
    {
        // Each line is expected to be of the form:
        // someuser:realm:c30b86d219198b1ada5b63fbfa0818e3
        // where userName == "someuser", password == "c30b86d219198b1ada5b63fbfa0818e3"
        // and "realm" is the user's role that we ignore.

        std::vector<std::string> tokens;
        boost::split(tokens, line, boost::is_any_of(":"));
        if (tokens.size() != 3)
        {
            NX_WARNING(
                this,
                lm("Skipping malformed line number %1. Expecting line to have form <user>:<realm>:<digest>")
                    .arg(lineNumber));
            continue;
        }

        std::string userName = tokens.front();
        std::string digest = tokens.back();

        {
            QnMutexLocker lock(&m_mutex);
            m_credentials.emplace(userName.c_str(), digest.c_str());
        }
    }

    if (m_credentials.empty())
    {
        NX_WARNING(
            this,
            lm("Unable to parse any htdigest credentials. Authentication will fail."));
    }
}

} // namespace nx::network::http::server