#ifndef BUSINESS_EVENT_PARAMETERS_H
#define BUSINESS_EVENT_PARAMETERS_H

#include <QtCore/QStringList>

#include <business/business_fwd.h>

#include <utils/common/model_functions_fwd.h>
#include <utils/common/uuid.h>

struct QnBusinessEventParameters {
    enum Param 
    {
        // base for any event type
        EventTypeParam,
        EventTimestampParam,
        EventResourceParam,
        ActionResourceParam,
        
        // event specific params.
        InputPortIdParam,

        ReasonCodeParam,
        ReasonParamsEncodedParam,
        
        SourceParam,
        ConflictsParam,

        ParamCount
    };

    QnBusinessEventParameters();

    QnBusiness::EventType eventType;
    qint64 eventTimestamp;
    QnUuid eventResourceId;
    QnUuid actionResourceId;

    QnBusiness::EventReason reasonCode;
    QString reasonParamsEncoded;
    QString source;
    QStringList conflicts;
    QString inputPortId;

    // convert/serialize/deserialize functions

    static QnBusinessEventParameters unpack(const QByteArray& value);
    QByteArray pack() const;

    /** Hash for events aggregation. */
    QnUuid getParamsHash() const;

private:
    static int getParamIndex(const QString& key);
};

#define QnBusinessEventParameters_Fields (eventType)(eventTimestamp)(eventResourceId)(actionResourceId)\
    (reasonCode)(reasonParamsEncoded)(source)(conflicts)(inputPortId)

QN_FUSION_DECLARE_FUNCTIONS(QnBusinessEventParameters, (ubjson)(json)(eq));


#endif // BUSINESS_EVENT_PARAMETERS_H
