#include "common_settings.h"

#include <iterator>

#include <plugins/plugin_tools.h>
#include <nx/kit/debug.h>

namespace nx {
namespace sdk {

void* CommonSettings::queryInterface(const nxpl::NX_GUID& interfaceId)
{
    if (interfaceId == interfaceId)
        return this;

    return nullptr;
}

void CommonSettings::addSetting(const std::string& key, const std::string& value)
{
    NX_KIT_ASSERT(!key.empty());
    m_settings[key] = value;
}

void CommonSettings::clear()
{
    m_settings.clear();
}

int CommonSettings::count() const
{
    return m_settings.size();
}

const char* CommonSettings::key(int i) const
{
    if (i < 0 || i >= m_settings.size())
        return nullptr;

    auto position = m_settings.cbegin();
    std::advance(position, i);
    return position->first.c_str();
}

const char* CommonSettings::value(int i) const
{
    if (i < 0 || i >= m_settings.size())
        return nullptr;

    auto position = m_settings.cbegin();
    std::advance(position, i);
    return position->second.c_str();
}

const char* CommonSettings::value(const char* key) const
{
    if (key == nullptr)
        return nullptr;

    Map::const_iterator it = m_settings.find(key);
    if (it == m_settings.cend())
        return nullptr;

    return it->second.c_str();
}

} // namespace sdk
} // namespace nx

