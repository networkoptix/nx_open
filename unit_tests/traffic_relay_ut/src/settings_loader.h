#pragma once

#include <map>
#include <string>

#include <nx/cloud/relay/settings.h>

namespace nx {
namespace cloud {
namespace relay {

class SettingsLoader
{
public:
    void addArg(const std::string& name, const std::string& value);

    const conf::Settings& settings() const;

    void load();

private:
    std::map<std::string, std::string> m_args;
    conf::Settings m_settings;
};

} // namespace relay
} // namespace cloud
} // namespace nx
