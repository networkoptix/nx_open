// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "object_type_dictionary.h"
#include "abstract_state_watcher.h"

namespace nx::analytics::taxonomy {

ObjectTypeDictionary::ObjectTypeDictionary(AbstractStateWatcher* taxonomyWatcher, QObject* parent):
    QObject(parent),
    m_taxonomyWatcher(taxonomyWatcher)
{
}

std::optional<QString> ObjectTypeDictionary::idToName(const QString& id) const
{
    const auto state = m_taxonomyWatcher ? m_taxonomyWatcher->state() : nullptr;
    const auto objectType = state ? state->objectTypeById(id) : nullptr;
    if (objectType)
        return objectType->name();

    return std::nullopt;
}

} // namespace nx::analytics::taxonomy
