// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "migration_utils.h"

#include <nx/utils/log/assert.h>
#include <nx/utils/range_adapters.h>

namespace nx::vms::event {

EventType convertToOldEvent(const QString& typeId)
{
    static const auto eventTypeMap =
        []
        {
            QMap<QStringView, EventType> result;
            for (auto [oldType, newType]: nx::utils::keyValueRange(eventTypesMap))
                result[newType] = oldType;

            return result;
        }();

    const auto result = eventTypeMap.value(typeId, EventType::undefinedEvent);
    NX_ASSERT(result != EventType::undefinedEvent);

    return result;
}

QString convertToNewEvent(const EventType& eventType)
{
    static const auto eventTypeMap =
        []
        {
            QMap<EventType, QStringView> result;
            for (auto [oldType, newType]: nx::utils::keyValueRange(eventTypesMap))
                result[oldType] = newType;

            return result;
        }();

    const auto result = eventTypeMap.value(eventType);
    NX_ASSERT(!result.isEmpty());

    return result.toString();
}

} // namespace nx::vms::event
