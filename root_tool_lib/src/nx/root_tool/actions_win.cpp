#include "actions.h"

namespace nx {
namespace root_tool {

bool Actions::mount(const std::string& /*url*/, const std::string& /*directory*/,
    const boost::optional<std::string>& /*username*/, const boost::optional<std::string>& /*password*/)
{
    return false;
}

bool Actions::unmount(const std::string& /*directory*/)
{
    return false;
}

bool Actions::changeOwner(const std::string& /*path*/)
{
    return false;
}

bool Actions::touchFile(const std::string& /*filePath*/)
{
    return false;
}

bool Actions::makeDirectory(const std::string& /*directoryPath*/);
{
    return false;
}

bool Actions::install(const std::string& /*debPackage*/)
{
    return false;
}

void Actions::showIds()
{

}

bool Actions::setupIds()
{
    return false;
}

std::string Actions::lastError() const
{
    return "";
}

} // namespace root_tool
} // namespace nx