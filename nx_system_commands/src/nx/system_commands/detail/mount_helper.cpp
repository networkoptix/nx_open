#include "mount_helper.h"
#include <sstream>
#include <cctype>

namespace nx {
namespace system_commands {

MountHelperBase::MountHelperBase(
    const boost::optional<std::string>& username,
    const boost::optional<std::string>& password)
    :
    m_username(username ? *username : "guest"),
    m_password(password ? *password : "")
{
    if (username && !checkAndParseUsername())
        m_invalidUsername = true;
}

SystemCommands::MountCode MountHelperBase::mount(
    const std::string& url,
    const std::string& directory)
{
    if (!isMountPathAllowed(directory))
        return SystemCommands::MountCode::otherError;

    if (m_invalidUsername)
        return SystemCommands::MountCode::otherError;

    m_url = url;
    m_directory = directory;

    if (m_url.size() < 3 || m_url[0] != '/' || m_url[1] != '/' || !isalnum(m_url[2]))
        return SystemCommands::MountCode::otherError;

    for (const auto& domain: m_domains)
        tryMountWithDomain(domain);

    return m_hasCredentialsError ? SystemCommands::MountCode::wrongCredentials : m_result;
}

bool MountHelperBase::checkAndParseUsername()
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

void MountHelperBase::tryMountWithDomain(const std::string& domain)
{
    if (m_result == SystemCommands::MountCode::ok)
        return;

    for (const auto& password: {m_password, std::string("123")})
        tryMountWithDomainAndPassword(domain, password);
}

void MountHelperBase::tryMountWithDomainAndPassword(
    const std::string& domain,
    const std::string& password)
{
    if (m_result == SystemCommands::MountCode::ok)
        return;

    auto credentialsFile = credentialsFileName(m_username, password);
    if (credentialsFile.empty())
        return;

    for (const auto& ver: {"", "1.0", "2.0"})
    {
        m_result = osMount(makeCommandString(domain, ver, credentialsFile));
        if (m_result == SystemCommands::MountCode::ok)
            break;

        if (m_result == SystemCommands::MountCode::wrongCredentials)
            m_hasCredentialsError = true;
    }
}

std::string MountHelperBase::makeCommandString(
    const std::string& domain,
    const std::string& ver,
    const std::string& credentialFile)
{
    std::ostringstream ss;
    ss << "mount -t cifs '" << m_url << "' '" << m_directory << "'"
        << " -o uid=" << uid() << ",gid=" << gid()
        << ",credentials=" << credentialFile;

    if (!domain.empty())
        ss << ",domain=" << domain;

    if (!ver.empty())
        ss << ",vers=" << ver;

    return ss.str();
}

} // namespace system_commands
} // namespace nx
