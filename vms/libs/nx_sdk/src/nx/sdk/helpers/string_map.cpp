// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "string_map.h"

#include <iterator>

#include <nx/kit/debug.h>

namespace nx::sdk {

void StringMap::setItem(const std::string& key, const std::string& value)
{
    NX_KIT_ASSERT(!key.empty());
    m_map[key] = value;
}

void StringMap::clear()
{
    m_map.clear();
}

int StringMap::count() const
{
    return (int) m_map.size();
}

const char* StringMap::key(int i) const
{
    if (i < 0 || i >= (int) m_map.size())
        return nullptr;

    auto position = m_map.cbegin();
    std::advance(position, i);
    return position->first.c_str();
}

const char* StringMap::value(int i) const
{
    if (i < 0 || i >= (int) m_map.size())
        return nullptr;

    auto position = m_map.cbegin();
    std::advance(position, i);
    return position->second.c_str();
}

const char* StringMap::value(const char* key) const
{
    if (key == nullptr)
        return nullptr;

    const Map::const_iterator it = m_map.find(key);
    if (it == m_map.cend())
        return nullptr;

    return it->second.c_str();
}

} // namespace nx::sdk
