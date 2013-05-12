#ifndef BUSINESS_EVENT_PARAMETERS_H
#define BUSINESS_EVENT_PARAMETERS_H

#include <business/business_logic_common.h>
#include "utils/common/qnid.h"

class QnBusinessEventParameters
{
public:

    enum Params 
    {
        // base for any event type
        eventTypeParam,
        eventTimestampParam,
        eventResourceParam,
        actionResourceParam,
        
        // event specific params.
        inputPortIdParam,

        reasonCodeParam,
        reasonTextParam,
        
        sourceParam,
        conflictsParam,

        CountParam
    };

    QnBusinessEventParameters();

    BusinessEventType::Value getEventType() const;
    void setEventType(BusinessEventType::Value value);

    qint64 getEventTimestamp() const;
    void setEventTimestamp(qint64 value);

    int getEventResourceId() const;
    void setEventResourceId(int value);

    int getActionResourceId() const;
    void setActionResourceId(int value);

    QnBusiness::EventReason getReasonCode() const;
    void setReasonCode(QnBusiness::EventReason value);

    QString getReasonText() const;
    void setReasonText(const QString& value);

    QString getSource() const;
    void setSource(QString value);

    QStringList getConflicts() const;
    void setConflicts(QStringList value);

    QString getInputPortId() const;
    void setInputPortId(const QString &value);


    // convert/serialize/deserialize functions

    static QnBusinessEventParameters deserialize(const QByteArray& value);
    QByteArray serialize() const;

    QnBusinessParams toBusinessParams() const;
    static QnBusinessEventParameters fromBusinessParams(const QnBusinessParams& bParams);

    //QVariant& operator[](int index);
    //const QVariant& operator[](int index) const;
    bool operator==(const QnBusinessEventParameters& other) const;

    QString getParamsKey() const;

private:
    static int getParamIndex(const QString& key);
private:
    BusinessEventType::Value m_eventType;
    qint64 m_timestamp;
    QnId m_resourceId;
    QnId m_actionResourceId;

    QString m_inputPort;
    QnBusiness::EventReason m_reasonCode;
    QString m_reasonText;
    QString m_source;
    QStringList m_conflicts;
};

#endif // BUSINESS_EVENT_PARAMETERS_H
