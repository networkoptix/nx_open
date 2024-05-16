// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "state_view.h"

#include <unordered_set>

#include <nx/utils/log/assert.h>
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
    std::unordered_set<ObjectType*> visitedObjects;
    std::function<ObjectType*(const std::vector<ObjectType*>&)> findObjectRecursivelyByFullName =
        [&](const std::vector<ObjectType*>& objectTypes) -> ObjectType*
        {
            for (auto objectType: objectTypes)
            {
                if (!NX_ASSERT(!visitedObjects.contains(objectType)))
                    break;

                visitedObjects.insert(objectType);

                if (objectType->mainTypeId() == objectTypeId)
                    return objectType;

                if (const auto result =
                    findObjectRecursivelyByFullName(objectType->derivedObjectTypes()))
                {
                    return result;
                }
            }

        return nullptr;
    };

    std::function<ObjectType*(const std::vector<ObjectType*>&)> findObjectRecursivelyByPart =
        [&](const std::vector<ObjectType*>& objectTypes) -> ObjectType*
        {
            for (const auto objectType: objectTypes)
            {
                if (!NX_ASSERT(!visitedObjects.contains(objectType)))
                    break;

                visitedObjects.insert(objectType);

                if (utils::contains(objectType->typeIds(), objectTypeId))
                    return objectType;

                if (auto result = findObjectRecursivelyByPart(objectType->derivedObjectTypes()))
                    return result;
            }

            return nullptr;
        };

    if (const auto resultByFullName = findObjectRecursivelyByFullName(m_objectTypes))
        return resultByFullName;

    visitedObjects.clear();

    return findObjectRecursivelyByPart(m_objectTypes);
}

} // namespace nx::vms::client::core::analytics::taxonomy
