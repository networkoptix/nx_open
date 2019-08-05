#include "system_commands.h"
#include <QtCore/QDir>
#include <QtCore/QString>

#include <set>
#include <iostream>
#include <sstream>
#include <stdio.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/statvfs.h>

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

} // namespace

SystemCommands::MountCode SystemCommands::mount(const std::string& /*url*/,
    const std::string& /*directory*/, const boost::optional<std::string>& /*username*/,
    const boost::optional<std::string>& /*password*/)
{
    return MountCode::otherError;
}

SystemCommands::UnmountCode SystemCommands::unmount(const std::string& /*directory*/)
{
    return UnmountCode::noPermissions;
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

std::string SystemCommands::lastError() const
{
    return m_lastError;
}

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
        "/sbin", "/snap", "/srv", "/sys", "/usr", "/var", "/private/var/vm", "/Volumes/Recovery"};

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
    const std::string& command,
    std::function<void(const char*)> outputAction)
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

bool SystemCommands::removePath(const std::string& path)
{
    if (!checkOwnerPermissions(path) || !execute("rm -rf '" + path + "'"))
        return false;

    return true;
}

int SystemCommands::open(const std::string& path, int mode)
{
    int fd = ::open(path.c_str(), mode, 0660);
    if (fd < 0)
        perror(format("open %", path).c_str());

    return fd;
}

bool SystemCommands::rename(const std::string& oldPath, const std::string& newPath)
{
    if (!checkOwnerPermissions(oldPath) || !checkOwnerPermissions(newPath))
        return false;

    return ::rename(oldPath.c_str(), newPath.c_str()) == 0;
}

int64_t SystemCommands::freeSpace(const std::string& path)
{
    struct statvfs stat;
    int64_t result = -1;

    if (statvfs(path.c_str(), &stat) == 0)
        result = stat.f_bavail * (int64_t) stat.f_bsize;
    else
        perror("freeSpace: statvfs64 failed:");

    return result;
}

int64_t SystemCommands::totalSpace(const std::string& path)
{
    struct statvfs stat;
    int64_t result = -1;

    if (statvfs(path.c_str(), &stat) == 0)
        result = stat.f_blocks * (int64_t) stat.f_frsize;
    else
    {
        char buf[1024];
        snprintf(buf, sizeof(buf), "totalSpace: statvfs failed for the path: %s", path.c_str());
        perror(buf);
    }

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
    return path;
}

std::string SystemCommands::serializedDmiInfo()
{
    return "";
}

} // namespace nx
