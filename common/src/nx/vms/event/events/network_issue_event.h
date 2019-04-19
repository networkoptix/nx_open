#pragma once

#include <nx/vms/event/events/reasoned_event.h>

#include <nx/fusion/model_functions_fwd.h>

namespace nx {
namespace vms {
namespace event {

class NetworkIssueEvent: public ReasonedEvent
{
    using base_type = ReasonedEvent;
public:
    struct MulticastAddressConflictParameters
    {
        SocketAddress address;
        QString addressUser;
    };
    #define MulticastAddressConflictParameters_Fields (address)(addressUser)

public:
    explicit NetworkIssueEvent(const QnResourcePtr& resource, qint64 timeStamp,
        EventReason reasonCode, const QString& reasonParamsEncoded);

    static int decodeTimeoutMsecs(const QString& encoded, const int defaultValue);
    static QString encodeTimeoutMsecs(int msecs);

    static bool decodePrimaryStream(const QString& encoded, const bool defaultValue);
    static QString encodePrimaryStream(bool isPrimary);

    struct PacketLossSequence
    {
        int prev;
        int next;
        bool valid;
    };

    static PacketLossSequence decodePacketLossSequence(const QString& encoded);
    static QString encodePacketLossSequence(int prev, int next);
};

QN_FUSION_DECLARE_FUNCTIONS(NetworkIssueEvent::MulticastAddressConflictParameters, (json))

} // namespace event
} // namespace vms
} // namespace nx
