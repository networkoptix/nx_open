// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "helpers.h"

namespace nx::analytics::taxonomy {

static void getAllDerivedTypeIds(
    const ObjectType* objectType,
    std::set<std::string>* inOutResult)
{
    for (const auto* derivedType: objectType->derivedTypes())
    {
        inOutResult->insert(derivedType->id());
        getAllDerivedTypeIds(derivedType, inOutResult);
    }
}

std::set<std::string> getAllDerivedTypeIds(const AbstractState* state, const std::string& typeId)
{
    std::set<std::string> result;

    const ObjectType* rootObjectType = state->objectTypeById(typeId);
    if (!rootObjectType)
        return result;

    getAllDerivedTypeIds(rootObjectType, &result);

    return result;
}

std::set<std::string> getAllBaseTypeIds(const AbstractState* state, const std::string& typeId)
{
    std::set<std::string> result;
    const auto* objectType = state->objectTypeById(typeId);
    if (!objectType)
        return result;

    while ((objectType = objectType->base()))
        result.insert(objectType->id());

    return result;
}

bool isBaseType(const AbstractState* state, const std::string& baseTypeId, const std::string& derivedTypeId)
{
    const auto* objectType = state->objectTypeById(derivedTypeId);
    if (!objectType)
        return false;

    while ((objectType = objectType->base()))
    {
        if (objectType->id() == baseTypeId)
            return true;
    }

    return false;
}

bool isTypeOrSubtypeOf(AbstractState* state, const std::string& typeId, const std::string& targetTypeId)
{
    return targetTypeId == typeId || isBaseType(state, typeId, targetTypeId);
}

} // namespace nx::analytics::taxonomy
