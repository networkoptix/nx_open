// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "helpers.h"

namespace nx::analytics::taxonomy {

static void getAllDerivedTypeIds(
    const AbstractObjectType* objectType,
    std::set<QString>* inOutResult)
{
    for (const AbstractObjectType* derivedType : objectType->derivedTypes())
    {
        inOutResult->insert(derivedType->id());
        getAllDerivedTypeIds(derivedType, inOutResult);
    }
}

std::set<QString> getAllDerivedTypeIds(const AbstractState* state, const QString& typeId)
{
    std::set<QString> result;

    const AbstractObjectType* rootObjectType = state->objectTypeById(typeId);
    if (!rootObjectType)
        return result;

    getAllDerivedTypeIds(rootObjectType, &result);

    return result;
}

std::set<QString> getAllBaseTypeIds(const AbstractState* state, const QString& typeId)
{
    std::set<QString> result;
    const AbstractObjectType* objectType = state->objectTypeById(typeId);
    if (!objectType)
        return result;

    while (objectType = objectType->base())
        result.insert(objectType->id());

    return result;
}

bool isBaseType(const AbstractState* state, const QString& baseTypeId, const QString& derivedTypeId)
{
    const AbstractObjectType* objectType = state->objectTypeById(derivedTypeId);
    if (!objectType)
        return false;

    while (objectType = objectType->base())
    {
        if (objectType->id() == baseTypeId)
            return true;
    }

    return false;
}

} // namespace nx::analytics::taxonomy
