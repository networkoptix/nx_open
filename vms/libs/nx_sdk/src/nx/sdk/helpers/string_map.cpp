// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "string_map.h"

#include <iterator>

#include <nx/kit/debug.h>

namespace nx::sdk {

void StringMap::setItem(std::string key, std::string value)
{
    NX_KIT_ASSERT(!key.empty());
    m_map[std::move(key)] = std::move(value);
    m_lookupCache.clear();
}

void StringMap::clear()
{
    m_map.clear();
    m_lookupCache.clear();
}

int StringMap::count() const
{
    return (int) m_map.size();
}

const char* StringMap::key(int i) const
{
    if (i < 0 || i >= (int) m_map.size())
        return nullptr;

    initLookupCache();
    return m_lookupCache[i * 2];
}

const char* StringMap::value(int i) const
{
    if (i < 0 || i >= (int) m_map.size())
        return nullptr;

    initLookupCache();
    return m_lookupCache[i * 2 + 1];
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

void StringMap::initLookupCache() const
{
    if (m_lookupCache.empty())
    {
        m_lookupCache.reserve(m_map.size() * 2);
        for (const auto& [key, value]: m_map)
        {
            m_lookupCache.push_back(key.c_str());
            m_lookupCache.push_back(value.c_str());
        }
    }
}

} // namespace nx::sdk
