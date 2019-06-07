#include "mount_helper.h"
#include <sstream>
#include <cctype>
#include <iostream>

namespace nx {
namespace system_commands {

MountHelper::MountHelper(
    const boost::optional<std::string>& username,
    const boost::optional<std::string>& password,
    const MountHelperDelegates& delegates)
    :
    m_username(username ? *username : "guest"),
    m_password(password ? *password : ""),
    m_delegates(delegates)
{
    if (username && !checkAndParseUsername())
        m_invalidUsername = true;
}

SystemCommands::MountCode MountHelper::mount(
    const std::string& url,
    const std::string& directory)
{
    if (!m_delegates.isPathAllowed(directory))
        return SystemCommands::MountCode::otherError;

    if (m_invalidUsername)
        return SystemCommands::MountCode::otherError;

    m_url = url;
    m_directory = directory;

    if (m_url.size() < 3 || m_url[0] != '/' || m_url[1] != '/' || !isalnum(m_url[2]))
        return SystemCommands::MountCode::otherError;

    for (const auto& domain: m_domains)
        tryMountWithDomain(domain);

    if (m_result == SystemCommands::MountCode::ok)
        return m_result;

    return m_hasCredentialsError
        ? SystemCommands::MountCode::wrongCredentials
        : SystemCommands::MountCode::otherError;
}

bool MountHelper::checkAndParseUsername()
{
    auto domainSeparatorPos = m_username.find('\\');
    if (domainSeparatorPos == std::string::npos)
        return true;

    if (domainSeparatorPos == m_username.size() - 1 || domainSeparatorPos == 0)
        return false;

    std::string domain = m_username.substr(domainSeparatorPos + 1);
    m_username = m_username.substr(0, domainSeparatorPos);
    m_domains.push_back(domain);

    return true;
}

void MountHelper::tryMountWithDomain(const std::string& domain)
{
    if (m_result == SystemCommands::MountCode::ok)
        return;

    for (const auto& password: {m_password, std::string("123")})
        tryMountWithDomainAndPassword(domain, password);
}

void MountHelper::tryMountWithDomainAndPassword(
    const std::string& domain,
    const std::string& password)
{
    if (m_result == SystemCommands::MountCode::ok)
        return;

    auto credentialsFile = m_delegates.credentialsFileName(m_username, password);
    if (credentialsFile.empty())
        return;

    for (const auto& ver: {"", "1.0", "2.0"})
    {
        m_result = m_delegates.osMount(makeCommandString(domain, ver, credentialsFile));
        if (m_result == SystemCommands::MountCode::ok)
            break;

        if (m_result == SystemCommands::MountCode::wrongCredentials && m_password == password)
            m_hasCredentialsError = true;
    }
}

std::string MountHelper::makeCommandString(
    const std::string& domain,
    const std::string& ver,
    const std::string& credentialFile)
{
    std::ostringstream ss;
    ss << "mount -t cifs '" << m_url << "' '" << m_directory << "'"
        << " -o credentials=" << credentialFile;

    if (!domain.empty())
        ss << ",domain=" << domain;

    if (!ver.empty())
        ss << ",vers=" << ver;

    return ss.str();
}

} // namespace system_commands
} // namespace nx
