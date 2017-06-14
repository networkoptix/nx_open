#pragma once

#include <QtCore/QStringList>

#include <business/business_fwd.h>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/uuid.h>

struct QnEventMetaData
{
    /**
     * Camera list which is associated with the event. EventResourceId may be a POS terminal, but
     * this is a camera list which should be shown with this event.
     */
    std::vector<QnUuid> cameraRefs;

    //! Users that can generate this event. Empty == any user
    std::vector<QnUuid> instigators;

    QnEventMetaData() = default;
    QnEventMetaData(const QnEventMetaData&) = default;
    QnEventMetaData(QnEventMetaData&&) = default;
    QnEventMetaData& operator= (const QnEventMetaData&) = default;
    QnEventMetaData& operator= (QnEventMetaData&&) = default;
};
#define QnEventMetaData_Fields (cameraRefs)(instigators)
QN_FUSION_DECLARE_FUNCTIONS(QnEventMetaData, (ubjson)(json)(eq)(xml)(csv_record));

struct QnBusinessEventParameters
{
    QnBusinessEventParameters();

    QnBusiness::EventType eventType;

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
    QnBusiness::EventReason reasonCode;

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
    QnEventMetaData metadata;

    /**
     * Identifier of bookmark. Used to prevent double bookmark creation on creation confirmation.
     */
    QnUuid bookmarkId;

    /** Hash for events aggregation. */
    QnUuid getParamsHash() const;
};
#define QnBusinessEventParameters_Fields \
    (eventType)(eventTimestampUsec)(eventResourceId)(resourceName)(sourceServerId) \
    (reasonCode)(inputPortId)(caption)(description)(metadata)(bookmarkId)
QN_FUSION_DECLARE_FUNCTIONS(QnBusinessEventParameters, (ubjson)(json)(eq)(xml)(csv_record));
