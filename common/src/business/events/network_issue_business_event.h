#ifndef __NETWORK_ISSUE_BUSINESS_EVENT_H__
#define __NETWORK_ISSUE_BUSINESS_EVENT_H__

#include "reasoned_business_event.h"

class QnNetworkIssueBusinessEvent: public QnReasonedBusinessEvent
{
    typedef QnReasonedBusinessEvent base_type;
public:
    QnNetworkIssueBusinessEvent(const QnResourcePtr& resource,
                          qint64 timeStamp,
                          QnBusiness::EventReason reasonCode,
                          const QString &reasonParamsEncoded);

    static int decodeTimeoutMsecs(const QString &encoded, const int defaultValue);
    static QString encodeTimeoutMsecs(int msecs);

    static bool decodePrimaryStream(const QString &encoded, const bool defaultValue);
    static QString encodePrimaryStream(bool isPrimary);

    struct PacketLossSequence {
        int prev;
        int next;
        bool valid;
    };

    static PacketLossSequence decodePacketLossSequence(const QString &encoded);
    static QString encodePacketLossSequence(int prev, int next);
};

typedef QSharedPointer<QnNetworkIssueBusinessEvent> QnNetworkIssueBusinessEventPtr;

#endif // __NETWORK_ISSUE_BUSINESS_EVENT_H__
