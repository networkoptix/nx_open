#pragma once

#include <string>
#include <vector>
#include <functional>
#include <boost/optional.hpp>
#include "../../system_commands.h"

namespace nx {
namespace system_commands {

#ifdef WIN32
using uid_t = unsigned int;
#endif

class MountHelperBase
{
public:
    MountHelperBase(
        const boost::optional<std::string>& username,
        const boost::optional<std::string>& password);

    SystemCommands::MountCode mount(
        const std::string& url,
        const std::string& directory);

protected:
    std::string m_username;
    std::string m_password;
    std::vector<std::string> m_domains = { "WORKGROUP", "" };

private:
    std::string m_url;
    std::string m_directory;
    SystemCommands::MountCode m_result = SystemCommands::MountCode::otherError;
    bool m_hasCredentialsError = false;
    bool m_invalidUsername = false;

    virtual SystemCommands::MountCode osMount(const std::string& command) = 0;
    virtual bool isMountPathAllowed(const std::string& path) const = 0;
    virtual std::string credentialsFileName(
        const std::string& username,
        const std::string& password) const = 0;
    virtual uid_t gid() const = 0;
    virtual uid_t uid() const = 0;

    void tryMountWithDomain(const std::string& domain);
    void tryMountWithDomainAndPassword(const std::string& domain, const std::string& password);
    std::string makeCommandString(
        const std::string& domain,
        const std::string& ver,
        const std::string& credentialFile);
    bool checkAndParseUsername();
};

} // namespace system_commands
} // namespace nx
