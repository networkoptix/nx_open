// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QJsonValue>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/json/qjson.h>
#include <nx/utils/json/qt_containers_reflect.h>
#include <nx/vms/api/json/value_or_array.h>
#include <nx/vms/api/types/event_rule_types.h>

#include "../data/server_time_period.h"
#include "aggregated_info.h"
#include "event_log_fwd.h"
#include "event_log_v3.h"

namespace nx::vms::api::rules {

NX_REFLECTION_ENUM_CLASS(EventLogFlag,
    noFlags = 0,

    /**%apidoc The notification requires user acknowledge. */
    acknowledge = 1 << 0,

    /**%apidoc There is recording archive exists for the time and device of the event. */
    videoLinkExists = 1 << 1)
Q_DECLARE_FLAGS(EventLogFlags, EventLogFlag)

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
    nx::Uuid ruleId;

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

    /**%apidoc[opt] Event log record required flags. */
    EventLogFlags flags;

    EventLogFilter():
        ServerTimePeriod(ServerTimePeriod::infinite())
    {}

    EventLogFilter(const ServerTimePeriod& period):
        ServerTimePeriod(period)
    {}
};

#define EventLogFilter_Fields \
    ServerTimePeriod_Fields(eventResourceId)(eventType)(eventSubtype)(actionType)(ruleId)(text) \
    (eventsOnly)(order)(limit)(flags)

NX_REFLECTION_INSTRUMENT(EventLogFilter, EventLogFilter_Fields)
QN_FUSION_DECLARE_FUNCTIONS(EventLogFilter, (json), NX_VMS_API)

struct NX_VMS_API EventLogRecord
{
    /**%apidoc Event timestamp. Used for sorting responses from multiple servers.*/
    std::chrono::milliseconds timestampMs = std::chrono::milliseconds::zero();

    /**%apidoc Event data. Key is 'fieldName' from event manifest and value is serialized field
     * data. See /rest/v{4-}/events/manifest/events for event manifests.
     */
    QMap<QString, QJsonValue> eventData;

    /**%apidoc[opt] Action data. Key is 'fieldName' from action manifest and value is serialized field
     * data. See /rest/v{4-}/events/manifest/actions for action manifests.
     */
    QMap<QString, QJsonValue> actionData;

    /**%apidoc[opt] Set of events occurred during the rule aggregation period. Includes certain
     * number of events which happened first (except the very first one which is stored in the
     * `eventData` field), and some which happened last.
     */
    AggregatedInfo aggregatedInfo;

    /**%apidoc[opt] VMS rule id. */
    nx::Uuid ruleId;

    /**%apidoc[opt] Event log record flags. */
    EventLogFlags flags;
};

#define EventLogRecord_Fields \
    (timestampMs)(eventData)(actionData)(aggregatedInfo)(ruleId)(flags)
NX_REFLECTION_INSTRUMENT(EventLogRecord, EventLogRecord_Fields)
QN_FUSION_DECLARE_FUNCTIONS(EventLogRecord, (json)(sql_record), NX_VMS_API)

std::string NX_VMS_API toString(const EventLogRecord& record);

// Helper functions for log data sorting & merging.
inline quint64 getTimestamp(const EventLogRecord& record) { return record.timestampMs.count(); }
inline quint64 getLegacyTimestamp(const EventLogRecordV3& record)
{
    return record.eventParams.eventTimestampUsec / 1000;
}

} // namespace nx::vms::rules

// Enable fusion sql_record serialization.
void NX_VMS_API serialize_field(const nx::vms::api::rules::PropertyMap& data, QVariant* value);
void NX_VMS_API deserialize_field(const QVariant& value, nx::vms::api::rules::PropertyMap* data);
void NX_VMS_API serialize_field(const nx::vms::api::rules::AggregatedInfo& data, QVariant* value);
void NX_VMS_API deserialize_field(const QVariant& value, nx::vms::api::rules::AggregatedInfo* data);
