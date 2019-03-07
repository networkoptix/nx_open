#pragma once

#include <nx/vms/event/events/reasoned_event.h>

namespace nx {
namespace vms {
namespace event {

class NetworkIssueEvent: public ReasonedEvent
{
    using base_type = ReasonedEvent;

public:
    explicit NetworkIssueEvent(const QnResourcePtr& resource, qint64 timeStamp,
        EventReason reasonCode, const QString& reasonParamsEncoded);

    static int decodeTimeoutMsecs(const QString& encoded, const int defaultValue);
    static QString encodeTimeoutMsecs(int msecs);

    static bool decodePrimaryStream(const QString& encoded, const bool defaultValue);
    static QString encodePrimaryStream(bool isPrimary);

    struct PacketLoss
    {
        size_t prev = 0;
        size_t next = 0;
        size_t aggregated = 0;

        QString toString() const;
        static std::optional<PacketLoss> parse(const QString& string);
    };
};

} // namespace event
} // namespace vms
} // namespace nx
