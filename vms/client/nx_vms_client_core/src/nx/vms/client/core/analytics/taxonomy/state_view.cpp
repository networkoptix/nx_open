// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "state_view.h"

#include <nx/utils/std/algorithm.h>

#include "object_type.h"

namespace nx::vms::client::core::analytics::taxonomy {

StateView::StateView(std::vector<ObjectType*> objectTypes, QObject* parent):
    QObject(parent),
    m_objectTypes(objectTypes)
{
}

ObjectType* StateView::objectTypeById(const QString& objectTypeId) const
{
    for (auto objectType: m_objectTypes)
    {
        if (objectType->mainTypeId() == objectTypeId)
            return objectType;
    }

    for (auto objectType: m_objectTypes)
    {
        if (nx::utils::contains(objectType->typeIds(), objectTypeId))
            return objectType;
    }

    return nullptr;
}

} // namespace nx::vms::client::core::analytics::taxonomy
