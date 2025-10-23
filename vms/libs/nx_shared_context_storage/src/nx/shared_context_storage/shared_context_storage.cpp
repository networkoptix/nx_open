// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "shared_context_storage.h"

namespace nx::shared_context_storage {

void SharedContextStorage::setValue(std::string&& key, std::string&& value)
{
    std::lock_guard lock(m_storageMutex);
    m_storage[std::move(key)] = std::move(value);
}

bool SharedContextStorage::getValue(const std::string& key, std::string* value) const
{
    std::lock_guard lock(m_storageMutex);
    const auto it = m_storage.find(key);
    if (it == m_storage.end())
        return false;

    *value = it->second;
    return true;
}

bool SharedContextStorage::deleteData(const std::string& id, const std::string& name)
{
    std::lock_guard lock(m_storageMutex);
    return m_storage.erase(makeKey(id, name)) > 0;
}

std::string SharedContextStorage::makeKey(const std::string& id, const std::string& name)
{
    return id + ":" + name;
}

} // namespace nx::shared_context_storage
