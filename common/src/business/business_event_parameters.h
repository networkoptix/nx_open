#ifndef BUSINESS_EVENT_PARAMETERS_H
#define BUSINESS_EVENT_PARAMETERS_H

#include <QtCore/QStringList>

#include <business/business_fwd.h>
#include <utils/common/id.h>

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
        reasonParamsEncodedParam,
        
        sourceParam,
        conflictsParam,

        CountParam
    };

    QnBusinessEventParameters();

    QnBusiness::EventType getEventType() const;
    void setEventType(QnBusiness::EventType value);

    qint64 getEventTimestamp() const;
    void setEventTimestamp(qint64 value);

    QnId getEventResourceId() const;
    void setEventResourceId(const QnId& value);

    QnId getActionResourceId() const;
    void setActionResourceId(const QnId& value);

    QnBusiness::EventReason getReasonCode() const;
    void setReasonCode(QnBusiness::EventReason value);

    QString getReasonParamsEncoded() const;
    void setReasonParamsEncoded(const QString& value);

    QString getSource() const;
    void setSource(const QString& value);

    QStringList getConflicts() const;
    void setConflicts(const QStringList& value);

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
    QnBusiness::EventType m_eventType;
    qint64 m_timestamp;
    QnId m_resourceId;
    QnId m_actionResourceId;

    QString m_inputPort;
    QnBusiness::EventReason m_reasonCode;
    QString m_reasonParamsEncoded;
    QString m_source;
    QStringList m_conflicts;
};

#endif // BUSINESS_EVENT_PARAMETERS_H
