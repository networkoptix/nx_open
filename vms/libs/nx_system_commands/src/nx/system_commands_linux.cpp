#include "system_commands.h"
#include <nx/system_commands/domain_socket/send_linux.h>
#include <nx/system_commands/system_commands_ini.h>

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
#include <sys/resource.h>
#include <unistd.h>

#include <nx/kit/debug.h>

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

enum MountParamIndex
{
    Ind_username = 0,
    Ind_domain,
    Ind_password,
    Ind_ver,
    Ind_url,
    Ind_localPath,
};

using MountParams = std::tuple<
    std::optional<std::string>, std::optional<std::string>, std::optional<std::string>,
    std::string, std::string, std::string>;

namespace detail {

template<typename T>
void combineImpl(std::vector<T>& r, T& t)
{
    r.push_back(t);
}

template<typename T, typename Head, typename ... Tail>
void combineImpl(
    std::vector<T>& r, T& tuple, const std::vector<Head>& head, const std::vector<Tail>& ... tail)
{
    for (const auto& h: head)
    {
        constexpr size_t currentIndex = std::tuple_size_v<T> - sizeof...(Tail) - 1;
        std::get<currentIndex>(tuple) = h;
        combineImpl(r, tuple, tail...);
    }
}

} // namespace detail

template<typename... Args>
auto combine(const std::vector<Args>& ... args) -> std::vector<std::tuple<Args...>>
{
    std::vector<std::tuple<Args...>> result;
    std::tuple<Args...> t;
    detail::combineImpl(result, t, args...);
    return result;
}

std::string escapeSingleQuotes(const std::string& arg)
{
    std::stringstream ss;
    for (const char c: arg)
    {
        if (c == '\'')
            ss << "'\\''";
        else
            ss << c;
    }

    return ss.str();
}

std::string makeMountCommand(const MountParams& params)
{
    std::ostringstream ss;
    ss << "mount -t cifs '" << std::get<Ind_url>(params) << "' '" << std::get<Ind_localPath>(params) << "' -o ";

    if (std::get<Ind_username>(params))
        ss << " username='" << escapeSingleQuotes(*std::get<Ind_username>(params)) << "',";
    else
        ss << "guest,";

    if (std::get<Ind_password>(params))
        ss << "password='" << escapeSingleQuotes(*std::get<Ind_password>(params)) << "',";
    else
        ss << "password='',";

    if (std::get<Ind_domain>(params))
        ss << "domain='" << escapeSingleQuotes(*std::get<Ind_domain>(params)) << "',";

    ss << "vers=" << std::get<Ind_ver>(params);

    return ss.str();
}

std::pair<std::optional<std::string>, std::optional<std::string>> extractNameDomain(
    const std::optional<std::string>& username)
{
    static const std::string kWorkgroupDomain = "WORKGROUP";
    if (!username)
        return {std::nullopt, kWorkgroupDomain};

    const auto sep = username->find('\\');
    if (sep == std::string::npos
        || sep == username->size() - 1
        || sep == 0)
    {
        return {username, kWorkgroupDomain};
    }

    return {username->substr(0, sep), username->substr(sep + 1)};
}

auto generateMountParams(
    const std::string& url,
    const std::string& localPath,
    const std::optional<std::string>& maybeUsername,
    const std::optional<std::string>& maybePassword)
{
    #define vecS std::vector<std::string>
    #define vecMS std::vector<std::optional<std::string>>
    const auto [username, domain] = extractNameDomain(maybeUsername);
    const vecS versions = {
        /*Win10*/"3.1",
        /*Win7*/"2.1",
        /*Win2012, Win8*/"3.0",
        /*Win2008, Vista*/"2.0",
        /*NT, old linux kernels*/"1.0"
    };

    return combine(
        vecMS{username}, vecMS{domain, std::nullopt}, vecMS{maybePassword}, versions,
        vecS{url}, vecS{localPath});

    #undef vecS
    #undef vecMS
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

    for (const auto &allowed: {"/etc/nx_ini"})
    {
        if (path.find(allowed) == 0)
            return true;
    }

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
    const std::string& localPath,
    const std::optional<std::string>& maybeUsername,
    const std::optional<std::string>& maybePassword)
{
    if (!checkMountPermissions(localPath))
    {
        NX_OUTPUT << "It's forbidden to mount a remote folder to " << localPath;
        return SystemCommands::MountCode::otherError;
    }

    for (const auto& t: generateMountParams(url, localPath, maybeUsername, maybePassword))
    {
        const auto command = makeMountCommand(t);
        if (execute(command))
        {
            NX_OUTPUT << "Mount command \"" << command << "\" succeeded";
            return SystemCommands::MountCode::ok;
        }

        if (m_lastError.find("13") != std::string::npos)
        {
            NX_OUTPUT << "Mount command \"" << command << "\" failed with 'permission denied'";
            return SystemCommands::MountCode::wrongCredentials;
        }

        NX_OUTPUT << "Mount command \"" << command << "\" failed";
    }

    return SystemCommands::MountCode::otherError;
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
    else
        perror("freeSpace: statvfs64 failed:");

    return result;
}

int64_t SystemCommands::totalSpace(const std::string& path)
{
    struct statvfs64 stat;
    int64_t result = -1;

    if (statvfs64(path.c_str(), &stat) == 0)
        result = stat.f_blocks * (int64_t) stat.f_frsize;
    else
        perror("totalSpace: statvfs64 failed:");

    return result;
}

bool SystemCommands::isPathExists(const std::string& path)
{
    return stat(path).exists;
}

SystemCommands::Stats SystemCommands::stat(const std::string& path)
{
    struct stat64 buf;
    Stats result;
    result.exists = ::stat64(path.c_str(), &buf) == 0;

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
    struct stat64 statBuf;
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

        if (::stat64(pathBuf, &statBuf) != 0)
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
    struct stat64 statBuf;
    std::string result;
    if (::stat64(path.c_str(), &statBuf) == 0)
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

int SystemCommands::setFdLimit(int pid, int value)
{
    rlimit newLimit = {(rlim_t) value, (rlim_t) value};
    rlimit oldLimit;

    if (getrlimit(RLIMIT_NOFILE, &oldLimit) == -1)
    {
        NX_OUTPUT << "getrlimit failed. pid: " << pid << ", value: " << value;
        return -1;
    }

    if (prlimit(pid, RLIMIT_NOFILE, &newLimit, nullptr) == -1)
    {
        NX_OUTPUT << "prlimit failed. pid: " << pid << " value: " << value;
        return oldLimit.rlim_cur;
    }

    NX_OUTPUT << "prlimit succeeded. pid: " << pid << " value: " << value;
    return newLimit.rlim_cur;
}

std::string SystemCommands::lastError() const
{
    return m_lastError;
}

} // namespace nx
