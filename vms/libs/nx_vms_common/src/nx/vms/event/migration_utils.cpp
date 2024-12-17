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
            for (auto [oldType, newType]: nx::utils::constKeyValueRange(eventTypesMap))
                result[newType] = oldType;

            return result;
        }();

    const auto result = eventTypeMap.value(typeId, EventType::undefinedEvent);
    NX_ASSERT(result != EventType::undefinedEvent);

    return result;
}

QString convertToNewEvent(EventType eventType)
{
    return eventTypesMap.value(eventType);
}

ActionType convertToOldAction(const QString& typeId)
{
    static const auto actionTypeMap =
        []
        {
            QMap<QStringView, ActionType> result;
            for (auto [oldType, newType]: nx::utils::constKeyValueRange(actionTypesMap))
                result[newType] = oldType;

            return result;
        }();

    const auto result = actionTypeMap.value(typeId, ActionType::undefinedAction);
    NX_ASSERT(result != ActionType::undefinedAction);

    return result;
}

QString convertToNewAction(ActionType actionType)
{
    return actionTypesMap.value(actionType);
}

} // namespace nx::vms::event
