#include "system_commands.h"

namespace nx {

SystemCommands::MountCode SystemCommands::mount(const std::string& /*url*/,
    const std::string& /*directory*/, const std::optional<std::string>& /*username*/,
    const std::optional<std::string>& /*password*/)
{
    return MountCode::otherError;
}

SystemCommands::UnmountCode SystemCommands::unmount(const std::string& /*directory*/)
{
    return UnmountCode::noPermissions;
}

bool SystemCommands::changeOwner(
    const std::string& /*path*/,
    int /*uid*/,
    int /*gid*/,
    bool /*isRecursive*/)
{
    return true;
}

bool SystemCommands::makeReadable(const std::string& directoryPath)
{
    return false;
}

bool SystemCommands::makeDirectory(const std::string& directoryPath)
{
    return false;
}

std::string SystemCommands::lastError() const
{
    return "";
}

bool SystemCommands::checkMountPermissions(const std::string& /*directory*/)
{
    return true;
}

bool SystemCommands::checkOwnerPermissions(const std::string& /*path*/)
{
    return true;
}

int SystemCommands::execute(
    const std::string& /*command*/,
    std::function<void(const char*)> /*outputAction*/)
{
    return -1;
}

bool SystemCommands::removePath(const std::string& path)
{
    return false;
}

int SystemCommands::open(const std::string& /*path*/, int /*mode*/)
{
    return -1;
}

bool SystemCommands::rename(const std::string& oldPath, const std::string& newPath)
{
    return false;
}

int64_t SystemCommands::freeSpace(const std::string& path)
{
    return 0;
}

int64_t SystemCommands::totalSpace(const std::string& path)
{
    return 0;
}

bool SystemCommands::isPathExists(const std::string& /*path*/)
{
    return false;
}

SystemCommands::Stats SystemCommands::stat(const std::string& /*path*/)
{
    return SystemCommands::Stats();
}

std::string SystemCommands::serializedFileList(const std::string& /*path*/)
{
    return "";
}

int64_t SystemCommands::fileSize(const std::string& path)
{
    return 0;
}

std::string SystemCommands::devicePath(const std::string& path)
{
    return path;
}

std::string SystemCommands::serializedDmiInfo()
{
    return "";
}

int SystemCommands::setFdLimit(int /*pid*/, int /*value*/)
{
    return -1;
}


} // namespace nx
