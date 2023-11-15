// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "http_server_htdigest_authentication_provider.h"

#include <string>
#include <vector>

#include <boost/algorithm/string.hpp>

#include <nx/utils/log/log.h>
#include <nx/utils/system_error.h>

namespace nx::network::http::server {

HtdigestAuthenticationProvider::HtdigestAuthenticationProvider(const std::string& filePath)
{
    std::ifstream file(filePath);
    if (!file.is_open())
    {
        NX_ERROR(this, nx::format("Failed to open htdigest file: %1. Reason: %2. Authentication will fail.")
            .arg(filePath).arg(SystemError::getLastOSErrorText()));
        return;
    }

    NX_INFO(this, nx::format("Loading htdigest credentials from file: %1").arg(filePath));

    load(file);
    file.close();
}

HtdigestAuthenticationProvider::HtdigestAuthenticationProvider(std::istream& input)
{
    load(input);
}

void HtdigestAuthenticationProvider::getPasswordByUserName(
    const std::string& userName,
    LookupResultHandler completionHandler)
{
    PasswordLookupResult result;

    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        auto it = m_credentials.find(userName);

        if (it == m_credentials.end())
            result = Ha1LookupResultBuilder::build(PasswordLookupResult::Code::notFound);
        else
            result = Ha1LookupResultBuilder::build(it->second);
    }

    completionHandler(std::move(result));
}

std::vector<std::string> HtdigestAuthenticationProvider::usernames() const
{
    std::vector<std::string> result;

    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        result.reserve(m_credentials.size());
        for (const auto& [username, ha1]: m_credentials)
            result.push_back(username);
    }

    return result;
}

void HtdigestAuthenticationProvider::load(std::istream& input)
{
    std::string line;
    for (int lineNumber = 1; std::getline(input, line); ++lineNumber)
    {
        // Each line is expected to be of the form:
        // username:realm:ha1
        // where ha1 is MD5(username ":" realm ":" password)
        // and "realm" is the user's role that we ignore.

        std::vector<std::string> tokens;
        boost::split(tokens, line, boost::is_any_of(":"));
        if (tokens.size() != 3)
        {
            NX_WARNING(
                this,
                nx::format("Skipping malformed line number %1. Expecting line to have form <user>:<realm>:<digest>")
                    .arg(lineNumber));
            continue;
        }

        std::string userName = tokens.front();
        std::string digest = tokens.back();

        {
            NX_MUTEX_LOCKER lock(&m_mutex);
            m_credentials.emplace(userName, digest);
        }
    }

    if (m_credentials.empty())
    {
        NX_WARNING(
            this,
            nx::format("Unable to parse any htdigest credentials. Authentication will fail."));
    }
}

} // namespace nx::network::http::server
