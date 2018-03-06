#include <pwd.h>
#include <grp.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <set>
#include <assert.h>
#include "actions.h"


namespace nx {
namespace root_tool {

static const uid_t kRealUid = getuid();
static const uid_t kRealGid = getgid();

namespace {

std::string formatImpl(const char* pstr, std::stringstream& out)
{
    while (*pstr)
        out << *pstr;

    return out.str();
}

template<typename Head, typename... Tail>
std::string formatImpl(const char* pstr, std::stringstream& out, Head&& head, Tail&&... tail)
{
    const char* oldPstr = pstr;

    while (true)
    {
        while (*pstr && *pstr != '%')
        {
            out << *pstr;
            ++pstr;
        }

        if (!*pstr)
            return out.str();

        if (pstr != oldPstr && *(pstr - 1) == '\\')
        {
            out << *pstr;
            ++pstr;
            oldPstr = pstr;
            continue;
        }

        out << std::forward<Head>(head);
        ++pstr;

        return formatImpl(pstr, out, std::forward<Tail>(tail)...);
    }

    assert(false);
    return std::string();
}

template<typename... Args>
std::string format(const std::string& formatString, Args&&... args)
{
    std::stringstream out;
    const char* pstr = formatString.data();

    return formatImpl(pstr, out, std::forward<Args>(args)...);
}

} // namespace


bool Actions::checkMountPermissions(const std::string& directory)
{
    static const std::string kAllowedMountPointPreffix("/tmp/");
    static const std::string kAllowedMountPointSuffix("_temp_folder_");

    if (directory.find(kAllowedMountPointPreffix) != 0
        || directory.find(kAllowedMountPointSuffix) == std::string::npos)
    {
        m_lastError = format("Mount to % is forbidden", directory);
        return false;
    }

    return true;
}

bool Actions::checkOwnerPermissions(const std::string& path)
{
    static const std::set<std::string> kForbiddenOwnershipPaths = {"/"};
    static const std::set<std::string> kForbiddenOwnershipPrefixes = {
        "/bin", "/boot", "/cdrom", "/dev", "/etc", "/lib", "/lib64", "/proc", "/root", "/run",
        "/sbin", "/snap", "/srv", "/sys", "/usr", "/var"};

    for (const auto& forbiddenPath: kForbiddenOwnershipPaths)
    {
        if (path == forbiddenPath)
        {
            m_lastError = format("% is forbidden for chown", path);
            return false;
        }
    }

    for (const auto& forbiddenPrefix: kForbiddenOwnershipPrefixes)
    {
        if (path.find(forbiddenPrefix) == 0)
        {
            m_lastError = format("% is forbidden for chown", path);
            return false;
        }
    }

    return true;
}

bool Actions::execute(const std::string& command)
{
    const auto pipe = popen((command + " 2>&1").c_str(), "r");
    if (pipe == nullptr)
    {
        m_lastError = format("popen for % has failed", command);
        return false;
    }

    std::ostringstream output;
    char buffer[1024];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr)
        output << buffer;

    int retCode = pclose(pipe);
    if (retCode != 0)
    {
        m_lastError = format(
            "Command % failed with return code %. Output: %",
            command, retCode, output.str());
        return false;
    }

    return true;
}

Actions::CheckOwnerResult Actions::checkCurrentOwner(const std::string& url)
{
    struct stat info;
    if (stat(url.c_str(), &info))
    {
        m_lastError = format("stat() failed for %", url);
        return CheckOwnerResult::failed;
    }

    struct passwd* pw = getpwuid(info.st_uid);
    if (!pw)
    {
        m_lastError = format("getpwuid failed for %", url);
        return CheckOwnerResult::failed;
    }

    if (pw->pw_uid == kRealUid && pw->pw_gid == kRealGid)
        return CheckOwnerResult::real;

    return CheckOwnerResult::other;
}

bool Actions::mount(const std::string& url, const std::string& directory,
    const boost::optional<std::string>& username, const boost::optional<std::string>& password)
{
    if (!checkMountPermissions(directory))
        return false;

    if (url.find("//") != 0)
    {
        m_lastError = format("% is not an SMB url", url);
        return false;
    }

    auto makeCommandString =
        [&url, &directory](const std::string& username, const std::string& password)
        {
            std::ostringstream command;
            command << "mount -t cifs '" << url << "' '" << directory << "'"
                << " -o uid=" << kRealUid << ",gid=" << kRealGid
                <<",username=" << username << ",password=" << password;

            return command.str();
        };

    std::string passwordString = password ? *password : "";
    std::string usernameString = username ? *username : "guest";

    for (const auto& candidate: {usernameString, "WORKGROUP\\" + usernameString})
    {
        if (passwordString.empty())
        {
            for (const auto& passwordCandidate: {"", "123"})
            {
                if (execute(makeCommandString(candidate, passwordCandidate)))
                   return true;
            }
        }
        else
        {
            if (execute(makeCommandString(candidate, passwordString)))
               return true;
        }
    }

    return false;
}

bool Actions::unmount(const std::string& directory)
{
    if (!checkMountPermissions(directory))
        return false;

    // TODO: Check if it is mounted by cifs.
    return execute("umount '" + directory + "'");
}

bool Actions::changeOwner(const std::string& path)
{
    if (!checkOwnerPermissions(path))
        return false;

    std::ostringstream command;
    command << "chown -R " << kRealUid << ":" << kRealGid << " '" << path << "'";
    return execute(command.str());
}

bool Actions::touchFile(const std::string& filePath)
{
    if (!checkOwnerPermissions(filePath) || !execute("touch '" + filePath + "'"))
        return false;

    switch (checkCurrentOwner(filePath))
    {
        case CheckOwnerResult::real:
            return true;
        case CheckOwnerResult::other:
            return changeOwner(filePath);
        case CheckOwnerResult::failed:
            return false;
    }

    assert(false);
    return false;
}

bool Actions::makeDirectory(const std::string& directoryPath)
{
    if (!checkOwnerPermissions(directoryPath) || !execute("mkdir -p '" + directoryPath + "'"))
        return false;

    switch (checkCurrentOwner(directoryPath))
    {
        case CheckOwnerResult::real:
            return true;
        case CheckOwnerResult::other:
            return changeOwner(directoryPath);
        case CheckOwnerResult::failed:
            return false;
    }

    assert(false);
    return false;
}

bool Actions::install(const std::string& debPackage)
{
    // TODO: Check for deb package signature as soon as it is avaliable.

    return execute("dpkg -i '" + debPackage + "'");
}

void Actions::showIds()
{
    const auto w = std::setw(6);
    std::cout
        << "Real        UID:" << w << getuid() << ",  GID:" << w << getgid() << std::endl
        << "Effective   UID:" << w << geteuid() << ",  GID:" << w << getegid() << std::endl
        << "Setup       UID:" << w << kRealUid << ",  GID:" << w << kRealGid << std::endl;
}

bool Actions::setupIds()
{
    if (setreuid(geteuid(), geteuid()) != 0)
    {
        m_lastError = "setreuid has failed";
        return false;
    }

    if (setregid(getegid(), getegid()) != 0)
    {
        m_lastError = "setregid has failed";
        return false;
    }

    return true;
}

std::string Actions::lastError() const
{
    return m_lastError;
}

} // namespace root_tool
} // namespace nx
