// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <map>

#include <QtCore/QString>

namespace nx::analytics::taxonomy {

class Plugin;
class Engine;
class Group;
class EnumType;
class ColorType;
class ObjectType;
class EventType;

namespace details {

template<typename Map>
typename Map::mapped_type typeById(Map* typeMap, const QString& typeId)
{
    if (auto itr = typeMap->find(typeId); itr != typeMap->cend())
        return itr->second;

    return nullptr;
}

} // namespace details

struct InternalState
{
    std::map<QString, Plugin*> pluginById;
    std::map<QString, Engine*> engineById;
    std::map<QString, Group*> groupById;
    std::map<QString, EnumType*> enumTypeById;
    std::map<QString, ColorType*> colorTypeById;
    std::map<QString, ObjectType*> objectTypeById;
    std::map<QString, EventType*> eventTypeById;
    std::map<
        QString,
        std::vector<nx::vms::api::analytics::AttributeDescription>> attributeListById;

    template<typename Type>
    Type* getTypeById(const QString& id) const
    {
        if constexpr (std::is_same_v<Type, Plugin>)
            return details::typeById(&pluginById, id);
        if constexpr (std::is_same_v<Type, Engine>)
            return details::typeById(&engineById, id);
        if constexpr (std::is_same_v<Type, Group>)
            return details::typeById(&groupById, id);
        if constexpr (std::is_same_v<Type, EnumType>)
            return details::typeById(&enumTypeById, id);
        if constexpr (std::is_same_v<Type, ColorType>)
            return details::typeById(&colorTypeById, id);
        if constexpr (std::is_same_v<Type, ObjectType>)
            return details::typeById(&objectTypeById, id);
        if constexpr (std::is_same_v<Type, EventType>)
            return details::typeById(&eventTypeById, id);

        NX_ASSERT(false, "Unknown type: %1", typeid(Type));
        return nullptr;
    }
};

} // namespace nx::analytics::taxonomy
