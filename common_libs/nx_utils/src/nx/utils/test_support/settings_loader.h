#pragma once

#include <map>
#include <string>
#include <type_traits>

#include "../abstract_service_settings.h"
#include "../settings.h"

namespace nx {
namespace utils {
namespace test {

template<typename SettingsType>
class SettingsLoader
{
public:
    void addArg(const std::string& name, const std::string& value)
    {
        m_args[name] = value;
    }

    const SettingsType& settings() const
    {
        return m_settings;
    }

    void load()
    {
        std::vector<const char*> args;
        for (const auto& nameAndValue: m_args)
        {
            args.push_back(nameAndValue.first.c_str());
            args.push_back(nameAndValue.second.c_str());
        }

        m_settings.load((int)args.size(), args.data());
    }

private:
    std::map<std::string, std::string> m_args;
    SettingsType m_settings;
};

} // namespace test
} // namespace utils
} // namespace nx
