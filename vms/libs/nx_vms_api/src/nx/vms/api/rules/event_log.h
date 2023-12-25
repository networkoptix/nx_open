// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QJsonValue>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/vms/api/json/value_or_array.h>

#include "../data/server_time_period.h"
#include "common.h"
#include "event_log_fwd.h"

namespace nx::vms::api::rules {

struct NX_VMS_API EventLogFilter: public nx::vms::api::ServerTimePeriod
{
    /**%apidoc:stringArray List of event resource flexible ids. */
    std::optional<nx::vms::api::json::ValueOrArray<QString>> eventResourceId;

    /**%apidoc:stringArray
     * List of event types. See /rest/v{4-}/events/manifest/events for event manifests
     * with possible event types.
     */
    std::optional<nx::vms::api::json::ValueOrArray<QString>> eventType;

    /**%apidoc[opt]
     * Event subtype, for advanced analytics event filtering.
     * Analytics event type for 'Analytics event'.
     * Analytics object type for 'Analytics object detected' event.
     * See analytics taxonomy for the reference values.
     */
    QString eventSubtype;

    /**%apidoc:stringArray
     * List of action types. See /rest/v{4-}/events/manifest/actions for action manifests
     * with possible action types.
     */
    std::optional<nx::vms::api::json::ValueOrArray<QString>> actionType;

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

    /**%apidoc[opt] Event log record limit, zero value is no limit. */
    size_t limit = 0;

    EventLogFilter():
        ServerTimePeriod(ServerTimePeriod::infinite())
    {}

    EventLogFilter(const ServerTimePeriod& period):
        ServerTimePeriod(period)
    {}
};

#define EventLogFilter_Fields \
    ServerTimePeriod_Fields(eventResourceId)(eventType)(eventSubtype)(actionType)(ruleId)(text)(eventsOnly)(order)(limit)

NX_REFLECTION_INSTRUMENT(EventLogFilter, EventLogFilter_Fields)
QN_FUSION_DECLARE_FUNCTIONS(EventLogFilter, (json), NX_VMS_API)

struct NX_VMS_API EventLogRecord
{
    /**%apidoc Event timestamp. Used for sorting multiserver response.*/
    std::chrono::milliseconds timestampMs;

    /**%apidoc Event data. Key is 'fieldName' from event manifest and value is serialized field
     * data. See /rest/v{4-}/events/manifest/events for event manifests.
     */
    QMap<QString, QJsonValue> eventData;

    /**%apidoc[opt] Action data. Key is 'fieldName' from action manifest and value is serialized field
     * data. See /rest/v{4-}/events/manifest/actions for action manifests.
     */
    QMap<QString, QJsonValue> actionData;

    /**%apidoc[opt] Event aggregation count in the rule period. */
    size_t aggregationCount = 0;

    /**%apidoc[opt] VMS rule id. */
    QnUuid ruleId;
};

#define EventLogRecord_Fields \
    (timestampMs)(eventData)(actionData)(aggregationCount)(ruleId)

NX_REFLECTION_INSTRUMENT(EventLogRecord, EventLogRecord_Fields)
QN_FUSION_DECLARE_FUNCTIONS(EventLogRecord, (json)(sql_record), NX_VMS_API)

} // namespace nx::vms::rules

void NX_VMS_API serialize_field(const nx::vms::api::rules::PropertyMap& data, QVariant* value);
void NX_VMS_API deserialize_field(const QVariant& value, nx::vms::api::rules::PropertyMap* data);
