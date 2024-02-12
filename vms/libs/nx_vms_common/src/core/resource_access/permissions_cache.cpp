// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "permissions_cache.h"

#include <nx/utils/log/assert.h>

namespace {

static constexpr int kInitialTableRowSize = 4096;

} // namespace

namespace nx::core::access {

bool PermissionsCache::setPermissions(
    const nx::Uuid& subjectId,
    const nx::Uuid& resourceId,
    Qn::Permissions permissions)
{
    if (!NX_ASSERT(!subjectId.isNull() && !resourceId.isNull()))
        return false;

    const auto resourceIndex =
        [&]()
        {
            const auto indexItr = m_indexOfResource.find(resourceId);
            if (indexItr != m_indexOfResource.cend())
                return indexItr->second;

            if (!m_sparseColumns.empty())
            {
                const int index = m_sparseColumns.front();
                m_sparseColumns.pop_front();
                m_resourcesOrder[index] = resourceId;
                return index;
            }

            const int index = m_resourcesOrder.size();
            m_resourcesOrder.push_back(resourceId);
            m_indexOfResource.insert(std::make_pair(resourceId, index));
            return index;
        };

    const auto index = resourceIndex();

    auto& storageRow = m_storage[subjectId];
    if (storageRow.empty() && kInitialTableRowSize > index)
        storageRow.resize(kInitialTableRowSize, std::nullopt);

    if (storageRow.size() <= index)
        storageRow.resize(index + index / 2, std::nullopt);

    auto& record = storageRow[index];
    if (record && *record == permissions)
        return false;

    record = permissions;

    return true;
}

bool PermissionsCache::removePermissions(
    const nx::Uuid& subjectId,
    const nx::Uuid& resourceId)
{
    if (!NX_ASSERT(!subjectId.isNull() && !resourceId.isNull()))
        return false;

    auto indexItr = m_indexOfResource.find(resourceId);
    if (indexItr == m_indexOfResource.cend())
        return false;

    auto index = indexItr->second;

    auto rowItr = m_storage.find(subjectId);
    if (rowItr == m_storage.end())
        return false;

    auto& storageRow = rowItr->second;
    if (storageRow.size() <= index)
        return false;

    auto& record = storageRow[index];
    if (!record)
        return false;

    record.reset();
    return true;
}

PermissionsCache::ResourceIdsWithPermissions PermissionsCache::permissionsForSubject(
    const nx::Uuid& subjectId) const
{
    if (!NX_ASSERT(!subjectId.isNull()))
        return {};

    const auto rowItr = m_storage.find(subjectId);
    if (rowItr == m_storage.cend())
        return {};

    const auto& storageRow = rowItr->second;
    PermissionsCache::ResourceIdsWithPermissions result;
    for (int i = 0; i < std::min(m_resourcesOrder.size(), storageRow.size()); ++i)
    {
        if (!m_resourcesOrder.at(i).isNull())
            result.emplace_back(m_resourcesOrder.at(i), storageRow.at(i));
    }
    return result;
}

void PermissionsCache::removeSubject(const nx::Uuid& subjectId)
{
    if (!NX_ASSERT(!subjectId.isNull()))
        return;

    m_storage.erase(subjectId);

    // Subject Id may be User resource Id as well.
    removeResource(subjectId);
}

void PermissionsCache::removeResource(const nx::Uuid& resourceId)
{
    if (!NX_ASSERT(!resourceId.isNull()))
        return;

    auto indexItr = m_indexOfResource.find(resourceId);
    if (indexItr == m_indexOfResource.cend())
        return;

    const auto index = indexItr->second;

    m_resourcesOrder[index] = nx::Uuid();
    m_indexOfResource.erase(resourceId);

    auto rowItr = m_storage.begin();
    while (rowItr != m_storage.end())
    {
        if (rowItr->first == resourceId)
        {
            rowItr = m_storage.erase(rowItr); //< User resource as subject.
        }
        else
        {
            auto& storageRow = rowItr->second;
            if (storageRow.size() > index)
                storageRow[index].reset();
            rowItr++;
        }
    }

    if (index > m_resourcesOrder.size() / 2)
        m_sparseColumns.push_back(index);
    else
        m_sparseColumns.push_front(index);
}

std::optional<Qn::Permissions> PermissionsCache::permissions(
    const nx::Uuid& subjectId,
    const nx::Uuid& resourceId) const
{
    if (!NX_ASSERT(!subjectId.isNull() && !resourceId.isNull()))
        return {};

    auto indexItr = m_indexOfResource.find(resourceId);
    if (indexItr == m_indexOfResource.cend())
        return {};

    const auto index = indexItr->second;

    const auto rowItr = m_storage.find(subjectId);
    if (rowItr == m_storage.cend())
        return {};

    const auto& storageRow = rowItr->second;
    if (storageRow.size() <= index)
        return {};

    return storageRow.at(index);
}

void PermissionsCache::clear()
{
    m_storage.clear();
    m_resourcesOrder.clear();
    m_indexOfResource.clear();
    m_sparseColumns.clear();
}

} // namespace nx::core::access
