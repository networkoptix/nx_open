#include <pwd.h>
#include <grp.h>
#include <sys/stat.h>
#include <iomanip>
#include <unistd.h>
#include "actions.h"


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

enum class CheckOwnerResult
{
    real,
    other,
    failed,
};

static CheckOwnerResult checkCurrentOwner(Argument url)
{
    struct stat info;
    if (stat(url.c_str(), &info))
    {
        fprintf(stderr, "[Check owner] stat() failed for %s\n", url.c_str());
        return CheckOwnerResult::failed;
    }

    struct passwd* pw = getpwuid(info.st_uid);
    if (!pw)
    {
        fprintf(stderr, "[Check owner] getpwuid failed for %s\n", url.c_str());
        return CheckOwnerResult::failed;
    }

    if (pw->pw_uid == kRealUid && pw->pw_gid == kRealGid)
        return CheckOwnerResult::real;

    return CheckOwnerResult::other;
}

int mount(Argument url, Argument directory, OptionalArgument username, OptionalArgument password)
{
    checkMountPermissions(directory);
    if (url.find("//") != 0)
        throw std::invalid_argument(url + " is not an SMB url");

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
    std::exception_ptr mountExceptionPtr;

    for (const auto& candidate: {usernameString, "WORKGROUP\\" + usernameString})
    {
        try
        {
            if (execute(makeCommandString(candidate, passwordString)) == 0)
                return 0;
        }
        catch (const std::exception&)
        {
            mountExceptionPtr = std::current_exception();
        }
    }

    if (mountExceptionPtr)
        std::rethrow_exception(mountExceptionPtr);

    return -1;
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

    switch (checkCurrentOwner(filePath))
    {
        case CheckOwnerResult::real:
            return 0;
        case CheckOwnerResult::other:
            return changeOwner(filePath);
        case CheckOwnerResult::failed:
            return -1;
    }

    return 0;
}

int makeDirectory(Argument directoryPath)
{
    checkOwnerPermissions(directoryPath);
    execute("mkdir -p '" + directoryPath + "'"); //< TODO: use syscall

    switch (checkCurrentOwner(directoryPath))
    {
        case CheckOwnerResult::real:
            return 0;
        case CheckOwnerResult::other:
            return changeOwner(directoryPath);
        case CheckOwnerResult::failed:
            return -1;
    }

    return 0;
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
