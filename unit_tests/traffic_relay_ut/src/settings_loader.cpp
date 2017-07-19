#include "settings_loader.h"

#include <vector>

namespace nx {
namespace cloud {
namespace relay {

void SettingsLoader::addArg(const std::string& name, const std::string& value)
{
    m_args[name] = value;
}

const conf::Settings& SettingsLoader::settings() const
{
    return m_settings;
}

void SettingsLoader::load()
{
    std::vector<const char*> args;
    for (const auto& nameAndValue: m_args)
    {
        args.push_back(nameAndValue.first.c_str());
        args.push_back(nameAndValue.second.c_str());
    }

    m_settings.load((int)args.size(), args.data());
}

} // namespace relay
} // namespace cloud
} // namespace nx
