#include <string>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <iterator>
#include <set>
#include <assert.h>
#include <string.h>
#include <pwd.h>
#include <grp.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/statvfs.h>
#include <sys/mount.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include "system_commands.h"
#include "system_commands/domain_socket/detail/send_linux.h"

namespace nx {

static const uid_t kRealUid = getuid();
static const uid_t kRealGid = getgid();
const char* const SystemCommands::kDomainSocket = "/tmp/syscmd_socket3f64fa";

namespace {

std::string formatImpl(const char* pstr, std::stringstream& out)
{
    while (*pstr)
        out << *pstr++;

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

/**
 * This is formatting helper function which accepts format string with '%' as placeholders and any
 * number of arguments which might be put into the stringstream and produce string with substituted
 * arguments.
 */
template<typename... Args>
std::string format(const std::string& formatString, Args&&... args)
{
    std::stringstream out;
    const char* pstr = formatString.data();

    return formatImpl(pstr, out, std::forward<Args>(args)...);
}

} // namespace


bool SystemCommands::checkMountPermissions(const std::string& directory)
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

bool SystemCommands::checkOwnerPermissions(const std::string& path)
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

bool SystemCommands::execute(
    const std::string& command, std::function<void(const char*)> outputAction)
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
    {
        output << buffer;
        if (outputAction)
            outputAction(buffer);
    }

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

SystemCommands::CheckOwnerResult SystemCommands::checkCurrentOwner(const std::string& url)
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

SystemCommands::MountCode SystemCommands::mount(
    const std::string& url, const std::string& directory,
    const boost::optional<std::string>& username,
    const boost::optional<std::string>& password, bool reportViaSocket, int socketPostfix)
{
    MountCode result= MountCode::otherError;
    if (!checkMountPermissions(directory))
    {
        if (reportViaSocket)
            system_commands::domain_socket::detail::sendInt64(socketPostfix, (int64_t) result);

        return result;
    }

    if (url.find("//") != 0)
    {
        m_lastError = format("% is not an SMB url", url);
        if (reportViaSocket)
            system_commands::domain_socket::detail::sendInt64(socketPostfix, (int64_t) result);

        return result;
    }

    auto makeCommandString =
        [&url, &directory](const std::string& username, const std::string& password,
            const std::string& domain,
            const std::string& dialect)
        {
            std::ostringstream command;
            command << "mount -t cifs '" << url << "' '" << directory << "'"
                << " -o uid=" << kRealUid << ",gid=" << kRealGid
                << ",username=" << username << ",password=" << password;

            if (!domain.empty())
                command << ",domain=" << domain;

            if (!dialect.empty())
                command << ",vers=" << dialect;

            return command.str();
        };

    std::string passwordString = password ? *password : "";
    std::string userNameString = username ? *username : "guest";
    std::string userProvidedDomain;

    if (auto pos = userNameString.find("\\");
        pos != std::string::npos && pos != userNameString.size() - 1)
    {
        userProvidedDomain = userNameString.substr(pos + 1);
        userNameString = userNameString.substr(0, pos);
    }

    std::vector<std::string> domains = { "WORKGROUP", "" };
    if (!userProvidedDomain.empty())
        domains.push_back(userProvidedDomain);

    bool gotWrongCredentialsError = false;
    for (const auto& domain: domains)
    {
        for (const auto& passwordCandidate: {passwordString, std::string("123")})
        {
            for (const auto& dialect: {"", "2.0", "1.0"})
            {
                if (execute(makeCommandString(userNameString, passwordCandidate, domain, dialect)))
                {
                    result = MountCode::ok;
                    break;
                }
                else if (m_lastError.find("13") != std::string::npos) //< 'Permission denied' error code
                {
                    gotWrongCredentialsError = true;
                }
            }
        }
    }

    if (gotWrongCredentialsError && result != MountCode::ok)
        result = MountCode::wrongCredentials;

    if (reportViaSocket)
        system_commands::domain_socket::detail::sendInt64(socketPostfix, (int64_t) result);

    return result;
}

SystemCommands::UnmountCode SystemCommands::unmount(
    const std::string& directory, bool reportViaSocket, int socketPostfix)
{
    UnmountCode result = UnmountCode::noPermissions;

    if (!checkMountPermissions(directory))
    {
        result = UnmountCode::noPermissions;
    }
    else if (umount(directory.c_str()) == 0)
    {
        result = UnmountCode::ok;
    }
    else
    {
        switch(errno)
        {
            case EINVAL:
            case ENOENT:
                result = UnmountCode::notExists;
                break;
            case ENOMEM:
            case EBUSY:
                result = UnmountCode::busy;
                break;
            case EPERM:
                result = UnmountCode::noPermissions;
                break;
            default:
                assert(false);
        }
    }

    if (reportViaSocket)
        system_commands::domain_socket::detail::sendInt64(socketPostfix, (int64_t) result);

    return result;
}

bool SystemCommands::changeOwner(const std::string& path)
{
    if (!checkOwnerPermissions(path))
        return false;

    std::ostringstream command;
    command << "chown -R " << kRealUid << ":" << kRealGid << " '" << path << "'";
    return execute(command.str());
}

bool SystemCommands::touchFile(const std::string& filePath)
{
    if (!checkOwnerPermissions(filePath) || !execute("touch '" + filePath + "'"))
        return false;

    return true;
}

bool SystemCommands::makeDirectory(const std::string& directoryPath)
{
    if (!checkOwnerPermissions(directoryPath) || !execute("mkdir -p '" + directoryPath + "'"))
        return false;

    return true;
}

bool SystemCommands::removePath(const std::string& path)
{
    if (!checkOwnerPermissions(path) || !execute("rm -rf '" + path + "'"))
        return false;

    return true;
}

bool SystemCommands::rename(const std::string& oldPath, const std::string& newPath)
{
    if (!checkOwnerPermissions(oldPath) || !checkOwnerPermissions(newPath))
        return false;

    return ::rename(oldPath.c_str(), newPath.c_str()) == 0;
}

int SystemCommands::open(const std::string& path, int mode, bool reportViaSocket, int socketPostfix)
{
    auto lastSep = path.rfind('/');
    auto dirPath = path.substr(0, lastSep);

    if ((mode & O_CREAT) && !isPathExists(dirPath, false)
        && lastSep != std::string::npos && lastSep != 0)
    {
        makeDirectory(dirPath);
    }

    int fd = ::open(path.c_str(), mode, 0660);
    if (fd < 0)
        perror("open");

    if (reportViaSocket && fd > 0)
        system_commands::domain_socket::detail::sendFd(socketPostfix, fd);

    return fd;
}

int64_t SystemCommands::freeSpace(const std::string& path, bool reportViaSocket, int socketPostfix)
{
    struct statvfs64 stat;
    int64_t result = -1;

    if (statvfs64(path.c_str(), &stat) == 0)
        result = stat.f_bavail * (int64_t) stat.f_bsize;

    if (reportViaSocket)
        system_commands::domain_socket::detail::sendInt64(socketPostfix, result);

    return result;
}

int64_t SystemCommands::totalSpace(const std::string& path, bool reportViaSocket, int socketPostfix)
{
    struct statvfs64 stat;
    int64_t result = -1;

    if (statvfs64(path.c_str(), &stat) == 0)
        result = stat.f_blocks * (int64_t) stat.f_frsize;

    if (reportViaSocket)
        system_commands::domain_socket::detail::sendInt64(socketPostfix, result);

    return result;
}

bool SystemCommands::isPathExists(const std::string& path, bool reportViaSocket, int socketPostfix)
{
    struct stat buf;
    bool result = stat(path.c_str(), &buf) == 0;

    if (reportViaSocket)
        system_commands::domain_socket::detail::sendInt64(socketPostfix, (int64_t) result);

    return result;
}

std::string SystemCommands::serializedFileList(
    const std::string& path, bool reportViaSocket, int socketPostfix)
{
    DIR *dir = opendir(path.c_str());
    struct dirent *entry;
    struct stat statBuf;
    std::stringstream out;
    char pathBuf[2048];
    ssize_t pathBufLen;

    if (!dir)
    {
        if (reportViaSocket)
            system_commands::domain_socket::detail::sendBuffer(socketPostfix, "", 1);

        return "";
    }

    while ((entry = readdir(dir)) != NULL)
    {
        if (strcmp(entry->d_name, "..") == 0 || strcmp(entry->d_name, ".") == 0)
            continue;

        strncpy(pathBuf, path.c_str(), sizeof(pathBuf));
        pathBufLen = strlen(pathBuf);
        if (pathBufLen > 0 && pathBufLen < (ssize_t) sizeof(pathBuf)
            && pathBuf[pathBufLen - 1] != '/')
        {
            strcat(pathBuf, "/");
            pathBufLen += 1;
        }

        strncpy(pathBuf + pathBufLen, entry->d_name,
            std::max<int>((ssize_t) sizeof(pathBuf) - pathBufLen, 0));

        if (stat(pathBuf, &statBuf) != 0)
            continue;

        out << pathBuf << "," << (S_ISDIR(statBuf.st_mode) ? statBuf.st_size : 0) << ","
            << S_ISDIR(statBuf.st_mode) << ",";
    }

    closedir(dir);

    std::string result = out.str();
    if (reportViaSocket)
    {
        system_commands::domain_socket::detail::sendBuffer(
            socketPostfix, result.data(), result.size() + 1);
    }

    return result;
}

int64_t SystemCommands::fileSize(const std::string& path, bool reportViaSocket, int socketPostfix)
{
    struct stat buf;
    int64_t result = -1;

    if (stat(path.c_str(), &buf) == 0)
        result = buf.st_size;

    if (reportViaSocket)
        system_commands::domain_socket::detail::sendInt64(socketPostfix, result);

    return result;
}

std::string SystemCommands::devicePath(
    const std::string& path, bool reportViaSocket, int socketPostfix)
{
    struct stat statBuf;
    std::string result;
    if (stat(path.c_str(), &statBuf) == 0)
    {
        std::ifstream partitionsFile("/proc/partitions");
        if (partitionsFile.is_open())
        {
            std::ifstream::traits_type::int_type c;
            while ((c = partitionsFile.get()) != std::ifstream::traits_type::eof())
            {
                if (isdigit(c))
                {
                    partitionsFile.unget();
                    break;
                }
            }

            while (!partitionsFile.eof() && !partitionsFile.bad())
            {
                unsigned int majorVer, minorVer;
                int64_t size;
                partitionsFile >> majorVer >> minorVer >> size >> result;

                if (major(statBuf.st_dev) == majorVer && minor(statBuf.st_dev) == minorVer)
                {
                    result = "/dev/" + result;
                    break;
                }
            }
        }
    }

    if (reportViaSocket)
        system_commands::domain_socket::detail::sendBuffer(socketPostfix, result.data(), result.size());

    return result;
}

bool SystemCommands::kill(int pid)
{
    int result = ::kill(pid, SIGKILL);
    if (result != 0)
    {
        m_lastError = format("Kill failed for %", pid);
        return false;
    }

    return true;
}

std::string SystemCommands::serializedDmiInfo(bool reportViaSocket, int socketPostfix)
{
    constexpr std::array<const char*, 2> prefixes = {
       "Part Number: ",
       "Serial Number: ",
    };

    auto trim =
        [](std::string& str)
        {
            std::string::size_type pos = str.find_last_not_of(" \n\t");
            if (pos != std::string::npos)
            {
                str.erase(pos + 1);
                pos = str.find_first_not_of(' ');
                if (pos != std::string::npos)
                    str.erase(0, pos);
            }
            else
            {
                str.erase(str.begin(), str.end());
            }
        };

    std::string result;
    std::set<std::string> values[prefixes.size()];
    if (execute(
        "/usr/sbin/dmidecode -t17",
        [&values, &prefixes, trim](const char* line)
        {
            for (int index = 0; index < prefixes.size(); index++)
            {
                const char* ptr = strstr(line, prefixes[index]);
                if (ptr)
                {
                    std::string value = std::string(ptr + strlen(prefixes[index]));
                    trim(value);
                    values[index].insert(value);
                }
            }
        }))
    {
        for (size_t i = 0; i < prefixes.size(); ++i)
        {
            std::string tmp;
            for (const auto& value: values[i])
                tmp += value;

            for (size_t i = 0; i < sizeof(std::string::size_type); ++i)
                result.push_back('\0');

            std::string::size_type len = tmp.size();
            memcpy(
                result.data() + result.size() - sizeof(std::string::size_type), &len,
                sizeof(std::string::size_type));

            std::copy(tmp.cbegin(), tmp.cend(), std::back_inserter(result));
        }
    }

    if (reportViaSocket)
        system_commands::domain_socket::detail::sendBuffer(socketPostfix, result.data(), result.size());

    return result;
}

bool SystemCommands::install(const std::string& debPackage)
{
    // TODO: Check for deb package signature as soon as it is avaliable.

    return execute("dpkg -i '" + debPackage + "'");
}

void SystemCommands::showIds()
{
    const auto w = std::setw(6);
    std::cout
        << "Real        UID:" << w << getuid() << ",  GID:" << w << getgid() << std::endl
        << "Effective   UID:" << w << geteuid() << ",  GID:" << w << getegid() << std::endl
        << "Setup       UID:" << w << kRealUid << ",  GID:" << w << kRealGid << std::endl;
}

bool SystemCommands::setupIds()
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

std::string SystemCommands::lastError() const
{
    return m_lastError;
}

} // namespace nx
