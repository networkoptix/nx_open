#include "system_commands.h"

namespace nx {


bool SystemCommands::mount(const std::string& /*url*/, const std::string& /*directory*/,
    const boost::optional<std::string>& /*username*/, const boost::optional<std::string>& /*password*/)
{
    return false;
}

SystemCommands::UnmountCode SystemCommands::unmount(
    const std::string& /*directory*/, bool /*reportViaSocket*/)
{
    return noPermissions;
}

bool SystemCommands::changeOwner(const std::string& /*path*/)
{
    return false;
}

bool SystemCommands::touchFile(const std::string& /*filePath*/)
{
    return false;
}

bool SystemCommands::makeDirectory(const std::string& /*directoryPath*/)
{
    return false;
}

bool SystemCommands::install(const std::string& /*debPackage*/)
{
    return false;
}

void SystemCommands::showIds()
{
}

bool SystemCommands::setupIds()
{
    return false;
}

std::string SystemCommands::lastError() const
{
    return "";
}

bool SystemCommands::checkMountPermissions(const std::string& /*directory*/)
{
    return false;
}

bool SystemCommands::checkOwnerPermissions(const std::string& /*path*/)
{
    return false;
}

bool SystemCommands::execute(const std::string& /*command*/)
{
    return false;
}

SystemCommands::CheckOwnerResult SystemCommands::checkCurrentOwner(const std::string& /*url*/)
{
    return CheckOwnerResult::failed;
}

bool SystemCommands::removePath(const std::string& path)
{
    return false;
}

int SystemCommands::open(const std::string& path, int mode, bool usePipe)
{
    return -1;
}

bool SystemCommands::rename(const std::string& /*oldPath*/, const std::string& /*newPath*/)
{
    return false;
}

int64_t SystemCommands::freeSpace(const std::string& /*path*/, bool /*reportViaSocket*/)
{
    return -1;
}

int64_t SystemCommands::totalSpace(const std::string& /*path*/, bool /*reportViaSocket*/)
{
    return -1;
}

bool SystemCommands::isPathExists(const std::string& path, bool reportViaSocket)
{
    return false;
}

std::string SystemCommands::serializedFileList(const std::string& path, bool reportViaSocket)
{
    return "";
}

int64_t SystemCommands::fileSize(const std::string& path, bool reportViaSocket)
{
    return -1;
}

std::string SystemCommands::devicePath(const std::string& path, bool reportViaSocket)
{
    return "";
}

} // namespace nx