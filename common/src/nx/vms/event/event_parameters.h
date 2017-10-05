#pragma once

#include <QtCore/QStringList>

#include <nx/vms/event/event_fwd.h>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/uuid.h>

namespace nx {
namespace vms {
namespace event {

struct EventMetaData
{
    /**
     * Camera list which is associated with the event. EventResourceId may be a POS terminal, but
     * this is a camera list which should be shown with this event.
     */
    std::vector<QnUuid> cameraRefs;

    //! Users that can generate this event.
    std::vector<QnUuid> instigators;
    bool allUsers = false;

    EventMetaData() = default;
    EventMetaData(const EventMetaData&) = default;
    EventMetaData(EventMetaData&&) = default;
    EventMetaData& operator= (const EventMetaData&) = default;
    EventMetaData& operator= (EventMetaData&&) = default;
};

#define EventMetaData_Fields (cameraRefs)(instigators)(allUsers)
QN_FUSION_DECLARE_FUNCTIONS(EventMetaData, (ubjson)(json)(eq)(xml)(csv_record));

struct EventParameters
{
    EventParameters();

    EventType eventType;

    /** When did the event occur - in usecs. */
    qint64 eventTimestampUsec;

    /** Event source - camera or server. */
    QnUuid eventResourceId;

    /**
     * Name of the resource which caused the event. Used if no resource is actually registered in
     * the system. External custom event can provide some resource name with doesn't match
     * resourceId in the system. In this case resourceName is filled and resourceId remains empty.
     */
    QString resourceName;

#if 0
    /** Resource with was used for action. */
    QnUuid actionResourceId;
#endif // 0

    /** Server that generated the event. */
    QnUuid sourceServerId;

    /** Used for QnReasonedBusinessEvent business events as reason code. */
    EventReason reasonCode;

    /** Used for Input events only. */
    QString inputPortId;

    /**
     * Short event description. Used for camera/server conflict as resource name which cause error.
     * Used in custom events as a short description.
     */
    QString caption;

    /**
     * Long event description. Used for camera/server conflict as a long description (conflict
     * list). Used in ReasonedEvents as reason description. Used in custom events as a long
     * description.
     */
    QString description;

    /**
     * Camera list which is associated with the event. EventResourceId may be a POS terminal, but
     * this is a camera list which should be shown with this event.
     */
    EventMetaData metadata;

    // TODO: #GDM #vkutin #rvasilenko think about implementing something like std::variant here.
    QnUuid analyticsEventId() const;
    void setAnalyticsEventId(const QnUuid& id);
    QnUuid analyticsDriverId() const;
    void setAnalyticsDriverId(const QnUuid& id);

    /** Hash for events aggregation. */
    QnUuid getParamsHash() const;
};

#define EventParameters_Fields \
    (eventType)(eventTimestampUsec)(eventResourceId)(resourceName)(sourceServerId) \
    (reasonCode)(inputPortId)(caption)(description)(metadata)
QN_FUSION_DECLARE_FUNCTIONS(EventParameters, (ubjson)(json)(eq)(xml)(csv_record));

} // namespace event
} // namespace vms
} // namespace nx
