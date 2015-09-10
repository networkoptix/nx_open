#ifndef BUSINESS_EVENT_PARAMETERS_H
#define BUSINESS_EVENT_PARAMETERS_H

#include <QtCore/QStringList>

#include <business/business_fwd.h>

#include <utils/common/model_functions_fwd.h>
#include <utils/common/uuid.h>

struct QnBusinessEventParameters {

    QnBusinessEventParameters();

    QnBusiness::EventType eventType;

    /** When did the event occur - in usecs. */
    qint64 eventTimestampUsec;

    /** Event source - camera or server. */
    QnUuid eventResourceId;
    
    /* Resource name with cause a event. 
    /* resourceName is used if no resource actually registered in our system.
    /* External custom event can provide some resource name with doesn't match resourceId in our system. At this case resourceName is filled and resourceId stay empty
    */
    QString resourceName;

    //! Resource with was used for action
    //QnUuid actionResourceId;

    /** Server that generated the event. */
    QnUuid sourceServerId;

    //! Used for QnReasonedBusinessEvent business events as reason code.
    QnBusiness::EventReason reasonCode;

    //! Used for Input events only
    QString inputPortId;

    //! short event description. Used for camera/server conflict as resource name which cause error. Used in custom events as short description
    QString caption;    

    //! long event description. Used for camera/server conflict as long description (conflict list). Used in ReasonedEvents as reason description. Used in custom events as long description
    QString description;


    /** Hash for events aggregation. */
    QnUuid getParamsHash() const;
};

#define QnBusinessEventParameters_Fields (eventType)(eventTimestampUsec)(eventResourceId)(resourceName)(sourceServerId)\
    (reasonCode)(inputPortId)(caption)(description)

QN_FUSION_DECLARE_FUNCTIONS(QnBusinessEventParameters, (ubjson)(json)(eq)(xml)(csv_record));


#endif // BUSINESS_EVENT_PARAMETERS_H
