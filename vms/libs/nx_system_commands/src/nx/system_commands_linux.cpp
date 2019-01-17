#include "system_commands.h"
#include "system_commands/domain_socket/send_linux.h"
#include "system_commands/detail/mount_helper.h"

#include <algorithm>
#include <assert.h>
#include <dirent.h>
#include <fcntl.h>
#include <fstream>
#include <grp.h>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <functional>
#include <pwd.h>
#include <set>
#include <signal.h>
#include <sstream>
#include <string>
#include <string.h>
#include <sys/mount.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>

namespace nx {

#if defined(NX_CUSTOMIZATION_NAME)
    #define STRINGIFY2(v) #v
    #define STRINGIFY(v) STRINGIFY2(v)
    const char* const SystemCommands::kDomainSocket =
        "/tmp/" STRINGIFY(NX_CUSTOMIZATION_NAME) "_syscmd_socket3f64fa";
    #undef STRINGIFY
    #undef STRINGIFY2
#else
    const char* const SystemCommands::kDomainSocket = "/tmp/syscmd_socket3f64fa";
#endif

namespace {

static std::string formatImpl(const char* pstr, std::stringstream& out)
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

static std::string makeCredentialsFile(
    const std::string& userName, const std::string& password, std::string* outError)
{
    const std::string fileName = "/tmp/cifs_credentials_" + std::to_string(syscall(SYS_gettid));
    std::ofstream file(fileName.c_str());
    file << "username=" << userName << std::endl;
    file << "password=" << password << std::endl;
    if (!file.fail())
        return fileName;

    if (outError)
        *outError = "Unable to create credentials file: " + fileName;

    return "";
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

SystemCommands::MountCode SystemCommands::mount(
    const std::string& url,
    const std::string& directory,
    const boost::optional<std::string>& username,
    const boost::optional<std::string>& password)
{
    system_commands::MountHelperDelegates delegates;
    std::string credentialsFile;
    delegates.credentialsFileName =
        [this, &credentialsFile](const std::string& username, const std::string& password)
        {
            credentialsFile = makeCredentialsFile(username, password, &m_lastError);
            return credentialsFile;
        };
    delegates.isPathAllowed =
        [this](const std::string& path) { return checkMountPermissions(path); };
    delegates.osMount =
        [this](const std::string& command)
        {
            if (execute(command))
                return MountCode::ok;

            return (m_lastError.find("13") != std::string::npos) //< 'Permission denied' error code
                ? MountCode::wrongCredentials
                : MountCode::otherError;
        };

    system_commands::MountHelper mountHelper(username, password, delegates);
    auto result = mountHelper.mount(url, directory);
    if (!credentialsFile.empty())
        unlink(credentialsFile.c_str());
    return result;
}

SystemCommands::UnmountCode SystemCommands::unmount(const std::string& directory)
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
                result = UnmountCode::notMounted;
                break;
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

    return result;
}

bool SystemCommands::changeOwner(const std::string& path, int uid, int gid, bool isRecursive)
{
    if (!checkOwnerPermissions(path))
        return false;

    std::ostringstream command;
    command << "chown " << (isRecursive ? "-R " : "") << uid << ":" << gid << " '" << path << "'";

    return execute(command.str());
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

int SystemCommands::open(const std::string& path, int mode)
{
    int fd = ::open(path.c_str(), mode, 0660);
    if (fd < 0)
        perror("open");

    return fd;
}

int64_t SystemCommands::freeSpace(const std::string& path)
{
    struct statvfs64 stat;
    int64_t result = -1;

    if (statvfs64(path.c_str(), &stat) == 0)
        result = stat.f_bavail * (int64_t) stat.f_bsize;

    return result;
}

int64_t SystemCommands::totalSpace(const std::string& path)
{
    struct statvfs64 stat;
    int64_t result = -1;

    if (statvfs64(path.c_str(), &stat) == 0)
        result = stat.f_blocks * (int64_t) stat.f_frsize;

    return result;
}

bool SystemCommands::isPathExists(const std::string& path)
{
    return stat(path).exists;
}

SystemCommands::Stats SystemCommands::stat(const std::string& path)
{
    struct stat buf;
    Stats result;
    result.exists = ::stat(path.c_str(), &buf) == 0;

    if (result.exists)
    {
        if (S_ISDIR(buf.st_mode))
            result.type = Stats::FileType::directory;
        else if (S_ISREG(buf.st_mode))
            result.type = Stats::FileType::regular;
        else
            result.type = Stats::FileType::other;

        result.size = buf.st_size;
    }

    return result;
}

std::string SystemCommands::serializedFileList(const std::string& path)
{
    DIR *dir = opendir(path.c_str());
    struct dirent *entry;
    struct stat statBuf;
    std::stringstream out;
    char pathBuf[2048];
    ssize_t pathBufLen;

    if (!dir)
        return "";

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

        if (::stat(pathBuf, &statBuf) != 0)
            continue;

        out << pathBuf << "," << (S_ISDIR(statBuf.st_mode) ? statBuf.st_size : 0) << ","
            << S_ISDIR(statBuf.st_mode) << ",";
    }

    closedir(dir);

    std::string result = out.str();
    return result;
}

int64_t SystemCommands::fileSize(const std::string& path)
{
    return stat(path).size;
}

std::string SystemCommands::devicePath(const std::string& path)
{
    struct stat statBuf;
    std::string result;
    if (::stat(path.c_str(), &statBuf) == 0)
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

    return result;
}

std::string SystemCommands::serializedDmiInfo()
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

    return result;
}

std::string SystemCommands::lastError() const
{
    return m_lastError;
}

} // namespace nx
