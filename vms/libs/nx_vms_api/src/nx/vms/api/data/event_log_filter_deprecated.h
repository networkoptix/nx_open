// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QJsonValue>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/vms/api/json/value_or_array.h>
#include <nx/vms/api/types/event_rule_types.h>

#include "server_time_period.h"

namespace nx::vms::api {

struct NX_VMS_API EventLogFilterDeprecated: ServerTimePeriod
{
    /**%apidoc:stringArray List of event resource flexible ids. */
    std::optional<nx::vms::api::json::ValueOrArray<QString>> eventResourceId;

    /**%apidoc:stringArray List of event types. */
    std::optional<nx::vms::api::json::ValueOrArray<EventType>> eventType;

    /**%apidoc[opt]
     * Event subtype, for advanced analytics event filtering.
     * Analytics event type for 'Analytics event'.
     * Analytics object type for 'Analytics object detected' event.
     * See analytics taxonomy for the reference values.
     */
    QString eventSubtype;

    /**%apidoc[opt] */
    ActionType actionType = ActionType::undefinedAction;

    /**%apidoc[opt] VMS Rule id. */
    QnUuid ruleId;

    /**%apidoc[opt] Event description lookup string. */
    QString text;

    /**%apidoc[opt] Read event data only. */
    bool eventsOnly = false;

    /**%apidoc[opt]:enum
     * Event log record sort order.
     * %value asc
     * %value desc
     */
    Qt::SortOrder order = Qt::AscendingOrder;

    /**%apidoc[opt]:integer Event log record limit, zero value is no limit. */
    size_t limit = 0;

    EventLogFilterDeprecated():
        ServerTimePeriod(ServerTimePeriod::infinite())
    {}

    EventLogFilterDeprecated(const ServerTimePeriod& period):
        ServerTimePeriod(period)
    {}
};
#define EventLogFilterDeprecated_Fields \
    ServerTimePeriod_Fields \
    (eventResourceId) \
    (eventType) \
    (eventSubtype) \
    (actionType) \
    (ruleId) \
    (text) \
    (eventsOnly) \
    (order) \
    (limit)
NX_REFLECTION_INSTRUMENT(EventLogFilterDeprecated, EventLogFilterDeprecated_Fields)
QN_FUSION_DECLARE_FUNCTIONS(EventLogFilterDeprecated, (json), NX_VMS_API)

} // namespace nx::vms::api
