// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resourcenameset.h"

namespace nx::utils::stree {

bool ResourceNameSet::registerResource(int id, const std::string& name, QVariant::Type type)
{
    std::pair<ResNameToIDDict::iterator, bool> p = m_resNameToID.emplace(
        name, ResourceDescription(id, name, type));
    if (!p.second)
        return false;

    if (!m_resIDToName.emplace(id, p.first).second)
    {
        m_resNameToID.erase(p.first);
        return false;
    }

    return true;
}

ResourceNameSet::ResourceDescription ResourceNameSet::findResourceByName(
    const std::string& resName) const
{
    auto it = m_resNameToID.find(resName);
    return it == m_resNameToID.end()
        ? ResourceDescription()
        : it->second;
}

ResourceNameSet::ResourceDescription ResourceNameSet::findResourceByID(int resID) const
{
    auto it = m_resIDToName.find(resID);
    return it == m_resIDToName.end()
        ? ResourceDescription()
        : it->second->second;
}

void ResourceNameSet::removeResource(int resID)
{
    auto it = m_resIDToName.find(resID);
    if (it == m_resIDToName.end())
        return;
    m_resNameToID.erase(it->second);
    m_resIDToName.erase(it);
}

} // namespace nx::utils::stree
