#ifndef BUSINESS_EVENT_PARAMETERS_H
#define BUSINESS_EVENT_PARAMETERS_H

#include <QtCore/QStringList>

#include <business/business_fwd.h>
#include <utils/common/id.h>


class QnBusinessEventParameters
{
public:
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
        
        CaptionParam,
        DescriptionParam,

        ParamCount
    };

    QnBusinessEventParameters();

    QnBusiness::EventType getEventType() const;
    void setEventType(QnBusiness::EventType value);

    qint64 getEventTimestamp() const;
    void setEventTimestamp(qint64 value);

    QnUuid getEventResourceId() const;
    void setEventResourceId(const QnUuid& value);

    QnUuid getActionResourceId() const;
    void setActionResourceId(const QnUuid& value);

    QnBusiness::EventReason getReasonCode() const;
    void setReasonCode(QnBusiness::EventReason value);

    QString getReasonParamsEncoded() const;
    void setReasonParamsEncoded(const QString& value);

    QString getCaption() const;
    void setCaption(const QString& value);

    QString getDescription() const;
    void setDescription(const QString& value);

    QString getInputPortId() const;
    void setInputPortId(const QString &value);


    // convert/serialize/deserialize functions

    static QnBusinessEventParameters unpack(const QByteArray& value);
    QByteArray pack() const;

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
    QnUuid m_resourceId;
    QnUuid m_actionResourceId;

    QString m_inputPort;
    QnBusiness::EventReason m_reasonCode;
    QString m_reasonParamsEncoded;
    // short event description. Used for camera/server conflict as resource name which cause error. Used in custom events as short description
    QString m_caption;    
    // long event description. Used for camera/server conflict as long description (conflict list). Used in custom events as long description
    QString m_description;
};

#endif // BUSINESS_EVENT_PARAMETERS_H
