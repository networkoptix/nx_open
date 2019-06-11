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

struct MountHelperDelegates
{
    std::function<SystemCommands::MountCode(const std::string&)> osMount;
    std::function<bool(const std::string&)> isPathAllowed;
    std::function<std::string(const std::string&, const std::string&)> credentialsFileName;
};

class MountHelper
{
public:
    MountHelper(
        const boost::optional<std::string>& username,
        const boost::optional<std::string>& password,
        const MountHelperDelegates& delegates);

    SystemCommands::MountCode mount(
        const std::string& url,
        const std::string& directory);

protected:
    std::string m_username;
    const std::string m_password;
    std::vector<std::string> m_domains = { "WORKGROUP", "" };

private:
    std::string m_url;
    std::string m_directory;
    SystemCommands::MountCode m_result = SystemCommands::MountCode::otherError;
    bool m_hasCredentialsError = false;
    bool m_invalidUsername = false;
    MountHelperDelegates m_delegates;

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
