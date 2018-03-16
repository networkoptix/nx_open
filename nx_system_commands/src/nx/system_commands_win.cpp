#include "system_commands.h"

namespace nx {


bool SystemCommands::mount(const std::string& /*url*/, const std::string& /*directory*/,
    const boost::optional<std::string>& /*username*/, const boost::optional<std::string>& /*password*/)
{
    return false;
}

bool SystemCommands::unmount(const std::string& /*directory*/)
{
    return false;
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

} // namespace nx