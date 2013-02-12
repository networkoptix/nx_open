#ifndef REASONED_BUSINESS_EVENT_H
#define REASONED_BUSINESS_EVENT_H

#include "instant_business_event.h"

// TODO: WTF? In one place of business events module we use enums, in another - defines. 
// How about trying to write consistent code?
#define NETWORK_ISSUE_NO_FRAME 1
#define NETWORK_ISSUE_RTP_PACKET_LOST 2
#define MSERVER_TERMINATED 3
#define MSERVER_STARTED 4
#define STORAGE_IO_ERROR 5
#define STORAGE_NOT_ENOUGH_SPEED 6

namespace QnBusinessEventRuntime {

    int getReasonCode(const QnBusinessParams &params);
    void setReasonCode(QnBusinessParams* params, int value);

    QString getReasonText(const QnBusinessParams &params);
    void setReasonText(QnBusinessParams* params, QString value);
}

class QnReasonedBusinessEvent : public QnInstantBusinessEvent
{
    typedef QnInstantBusinessEvent base_type;
public:
    explicit QnReasonedBusinessEvent(const BusinessEventType::Value eventType,
                                     const QnResourcePtr& resource,
                                     const qint64 timeStamp,
                                     const int reasonCode,
                                     const QString& reasonText = QString());

    virtual QnBusinessParams getRuntimeParams() const override;
protected:
    int m_reasonCode;
    QString m_reasonText;
};

#endif // REASONED_BUSINESS_EVENT_H
