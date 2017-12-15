#include "actions.h"

#include <iomanip>

#include <unistd.h>

namespace nx {
namespace root_tool {

static const uid_t kRealUid = getuid();
static const uid_t kRealGid = getgid();

static void checkMountPermissions(const std::string& directory)
{
    static const std::string kAllowedMountPointPreffix("/tmp/");
    static const std::string kAllowedMountPointSuffix("_temp_folder_");

    if (directory.find(kAllowedMountPointPreffix) != 0
        || directory.find(kAllowedMountPointSuffix) == std::string::npos)
    {
        throw std::invalid_argument(directory + " is forbidden for mount");
    }
}

static void checkOwnerPermissions(const std::string& path)
{
    static const std::set<std::string> kForbiddenOwnershipPaths = {"/"};
    static const std::set<std::string> kForbiddenOwnershipPrefixes = {
        "/bin", "/boot", "/cdrom", "/dev", "/etc", "/lib", "/lib64", "/proc", "/root", "/run",
        "/sbin", "/snap", "/srv", "/sys", "/usr", "/var"};

    for (const auto& forbiddenPath: kForbiddenOwnershipPaths)
    {
        if (path == forbiddenPath)
            throw std::invalid_argument(path + " is forbidden for chown");
    }

    for (const auto& forbiddenPrefix: kForbiddenOwnershipPrefixes)
    {
        if (path.find(forbiddenPrefix) == 0)
            throw std::invalid_argument(path + " is forbidden for chown");
    }
}

static int execute(const std::string& command)
{
    const auto pipe = popen((command + " 2>&1").c_str(), "r");
    if (pipe == nullptr)
        throw std::runtime_error(command + " popen has failed");

    std::ostringstream output;
    char buffer[1024];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr)
        output << buffer;

    if (pclose(pipe) != 0)
        throw std::runtime_error(command + " -- " + output.str());

    return 0;
}

int mount(Argument url, Argument directory, OptionalArgument username, OptionalArgument password)
{
    checkMountPermissions(directory);
    if (url.find("//") != 0)
        throw std::invalid_argument(url + " is not an SMB url");

    std::ostringstream command;
    command << "mount -t cifs '" << url << "' '" << directory << "'"
       << " -o uid=" << kRealUid << ",gid=" << kRealGid;

    if (username)
        command << ",username=" << *username;

    if (password)
        command << ",password=" << *password;

    return execute(command.str());
}

int unmount(Argument directory)
{
    checkMountPermissions(directory);

    // TODO: Check if it is mounted by cifs.
    return execute("umount '" + directory + "'");
}

int changeOwner(Argument path)
{
    checkOwnerPermissions(path);

    std::ostringstream command;
    command << "chown -R " << kRealUid << ":" << kRealGid << " '" << path << "'";
    return execute(command.str());
}

int touchFile(Argument filePath)
{
    checkOwnerPermissions(filePath);
    execute("touch '" + filePath + "'"); //< TODO: use file open+close;
    return changeOwner(filePath);
}

int makeDirectory(Argument directoryPath)
{
    checkOwnerPermissions(directoryPath);
    execute("mkdir -p '" + directoryPath + "'"); //< TODO: use syscall
    return changeOwner(directoryPath);
}

int install(Argument debPackage)
{
    // TODO: Check for deb package signature as soon as it is avaliable.

    return execute("dpkg -i '" + debPackage + "'");
}

int showIds()
{
    const auto w = std::setw(6);
    std::cout
        << "Real        UID:" << w << getuid() << ",  GID:" << w << getgid() << std::endl
        << "Effective   UID:" << w << geteuid() << ",  GID:" << w << getegid() << std::endl
        << "Setup       UID:" << w << kRealUid << ",  GID:" << w << kRealGid << std::endl;

    return 0;
}

void setupIds()
{
    if (setreuid(geteuid(), geteuid()) != 0)
        throw std::runtime_error("setreuid has failed");

    if (setregid(getegid(), getegid()) != 0)
        throw std::runtime_error("setregid has failed");
}

} // namespace root_tool
} // namespace nx
