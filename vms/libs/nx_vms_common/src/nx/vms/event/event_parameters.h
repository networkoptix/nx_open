// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QStringList>

#include <analytics/common/object_metadata.h>
#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/instrument.h>
#include <nx/utils/uuid.h>
#include <nx/vms/event/event_fwd.h>

namespace nx {
namespace vms {
namespace event {

struct EventMetaData
{
    /**
     * Camera list which is associated with the event. EventResourceId may be a POS terminal, but
     * this is a camera list which should be shown with this event.
     */
    std::vector<QString> cameraRefs;

    //! Users that can generate this event.
    std::vector<QnUuid> instigators;
    bool allUsers = false;
    nx::vms::api::EventLevel level = nx::vms::api::EventLevel::UndefinedEventLevel;

    EventMetaData() = default;
    EventMetaData(const EventMetaData&) = default;
    EventMetaData(EventMetaData&&) = default;
    EventMetaData& operator= (const EventMetaData&) = default;
    EventMetaData& operator= (EventMetaData&&) = default;
    bool operator==(const EventMetaData& other) const = default;
};

#define EventMetaData_Fields (cameraRefs)(instigators)(allUsers)(level)
QN_FUSION_DECLARE_FUNCTIONS(EventMetaData, (ubjson)(json)(xml)(csv_record), NX_VMS_COMMON_API)

NX_REFLECTION_INSTRUMENT(EventMetaData, EventMetaData_Fields)

struct NX_VMS_COMMON_API EventParameters
{
    /**%apidoc Type of the Event. */
    EventType eventType = EventType::undefinedEvent;

    /**%apidoc When did the Event occur, in microseconds. */
    qint64 eventTimestampUsec = 0;

    /**%apidoc Event source - id of a Device, or a Server, or a PIR. */
    QnUuid eventResourceId;

    /**%apidoc
     * Name of the resource which caused the event. Used if no resource is actually registered in
     * the system. External custom event can provide some resource name with doesn't match
     * resourceId in the system. In this case resourceName is filled and resourceId remains empty.
     * In GUI and API referred as `source`.
     */
    QString resourceName;

    /**%apidoc Id of a Server that generated the Event. */
    QnUuid sourceServerId;

    /**%apidoc Used for Reasoned Events as reason code. */
    EventReason reasonCode = EventReason::none;

    /**%apidoc Used for Input Events only. Identifies the input port.
     * %// TODO: Refactor: inputPortId should not be used for analytics event type id.
     */
    QString inputPortId;

    /**%apidoc
     * Short event description. Used for camera/server conflict events as the resource name which
     * causes the error. Used in custom events as a short description.
     */
    QString caption;

    /**%apidoc
     * Long event description. Used for camera/server conflict as a long description (conflict
     * list). Used in ReasonedEvents as reason description. Used in custom events as a long
     * description.
     */
    QString description;

    /**%apidoc
     * Camera list which is associated with the event. EventResourceId may be a POS terminal, but
     * this is a camera list which should be shown with this event.
     * %// TODO: Fix the comment - metadata is more than a camera list.
     */
    EventMetaData metadata;

    /**
     * Flag allows to omit event logging to DB on the server.
     * This event still triggers user notifications
     */
    bool omitDbLogging = false;

    QString analyticsPluginId; //< #spanasenko: It's unused (?!).
    QnUuid analyticsEngineId;

    /** Used for Analytics Events. Takes part in ExternalUniqueKey along with EventTypeId. */
    QnUuid objectTrackId;

    /** Used for Analytics Events. Makes an additional component in ExternalUniqueKey. */
    QString key;

    nx::common::metadata::Attributes attributes;

    bool operator==(const EventParameters& other) const = default;

    // TODO: #sivanov #vkutin #rvasilenko Consider implementing via std::variant or similar.
    QString getAnalyticsEventTypeId() const;
    void setAnalyticsEventTypeId(const QString& id);

    QString getAnalyticsObjectTypeId() const;
    void setAnalyticsObjectTypeId(const QString& id);

    QnUuid getAnalyticsEngineId() const;
    void setAnalyticsEngineId(const QnUuid& id);

    QString getTriggerName() const;
    void setTriggerName(const QString& name);
    QString getTriggerIcon() const;
    void setTriggerIcon(const QString& icon);

    nx::vms::api::EventLevels getDiagnosticEventLevels() const;
    void setDiagnosticEventLevels(nx::vms::api::EventLevels levels);


    /** Hash for events aggregation. */
    QnUuid getParamsHash() const;
};

#define EventParameters_Fields \
    (eventType)(eventTimestampUsec)(eventResourceId)(resourceName)(sourceServerId) \
    (reasonCode)(inputPortId)(caption)(description)(metadata)(omitDbLogging)(analyticsPluginId) \
    (analyticsEngineId)(objectTrackId)(key)(attributes)
QN_FUSION_DECLARE_FUNCTIONS(EventParameters, (ubjson)(json)(xml)(csv_record), NX_VMS_COMMON_API);

NX_REFLECTION_INSTRUMENT(EventParameters, EventParameters_Fields)

bool checkForKeywords(const QString& value, const QString& keywords);

} // namespace event
} // namespace vms
} // namespace nx
