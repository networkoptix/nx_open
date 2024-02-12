// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "name_to_id.h"

namespace nx::vms::common::test {

QString NameToId::name(const nx::Uuid& id) const
{
    return m_idToName.value(id);
}

nx::Uuid NameToId::id(const QString& name) const
{
    return m_nameToId.value(name);
}

void NameToId::setId(const QString& name, const nx::Uuid& id)
{
    m_nameToId[name] = id;
    m_idToName[id] = name;
}

nx::Uuid NameToId::ensureId(const QString& name) const
{
    if (m_nameToId.contains(name))
        return id(name);

    const auto id = nx::Uuid::createUuid();
    m_nameToId[name] = id;
    m_idToName[id] = name;
    return id;
}

void NameToId::clear()
{
    m_nameToId.clear();
    m_idToName.clear();
}

NameToId::Ids NameToId::ensureIds(const Names& names) const
{
    Ids result;
    for (const auto& name: names)
        result.insert(ensureId(name));
    return result;
}

NameToId::Ids NameToId::ids(const Names& names) const
{
    Ids result;
    for (const auto& name: names)
        result.insert(id(name));
    return result;
}

NameToId::Names NameToId::names(const Ids& ids) const
{
    Names result;
    for (const auto& id: ids)
        result.insert(name(id));
    return result;
}

} // namespace nx::vms::common::test
