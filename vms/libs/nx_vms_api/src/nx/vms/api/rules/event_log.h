// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QJsonValue>

#include <nx/fusion/model_functions_fwd.h>

#include "../data/server_time_period.h"
#include "common.h"
#include "event_log_fwd.h"

namespace nx::vms::api::rules {

struct NX_VMS_API EventLogFilter
{
    /**%apidoc[opt] Period of event log timestamps, infinite by default. */
    ServerTimePeriod period;

    /**%apidoc[opt] List of event or action resource ids. */
    std::vector<QnUuid> resourceIds; //< TODO: #amalov Consider splitting to event/action resources.

    /**%apidoc[opt]
     * List of event types. See /rest/v{3-}/events/manifest/events for event manifests
     * with possible event types.
     */
    QStringList eventTypes;

    /**%apidoc[opt]
     * Event subtype, for advanced analytics event filtering.
     * Analytics event type for 'Analytics event'.
     * Analytics object type for 'Analytics object detected' event.
     * See analytics taxonomy for the reference values.
     */
    QString eventSubtype;

    /**%apidoc[opt]
     * List of action types. See /rest/v{3-}/events/manifest/actions for action manifests
     * with possible action types.
     */
    QStringList actionTypes;

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
};

struct NX_VMS_API EventLogRecord
{
    /**%apidoc Event data. Key is 'fieldName' from event manifest and value is serialized field
     * data. See /rest/v{3-}/events/manifest/events for event manifests.
     */
    PropertyMap eventData;

    /**%apidoc[opt] Action data. Key is 'fieldName' from action manifest and value is serialized field
     * data. See /rest/v{3-}/events/manifest/actions for action manifests.
     */
    PropertyMap actionData;

    /**%apidoc[opt] Event aggregation count in the rule period. */
    size_t aggregationCount = 0;

    /**%apidoc[opt] VMS rule id. */
    QnUuid ruleId;
};

#define EventLogRecord_Fields \
    (eventData)(actionData)(aggregationCount)(ruleId)

QN_FUSION_DECLARE_FUNCTIONS(EventLogRecord, (json)(sql_record), NX_VMS_API)

} // namespace nx::vms::rules
