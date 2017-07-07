#pragma once

#include <nx/vms/event/events/conflict_event.h>
#include <core/resource/resource_fwd.h>

class QHostAddress;
class QStringList;

namespace nx {
namespace vms {
namespace event {

class IpConflictEvent: public ConflictEvent
{
    using base_type = ConflictEvent;

public:
    IpConflictEvent(const QnResourcePtr& resource, const QHostAddress& address,
        const QStringList& macAddrList,  qint64 timeStamp);

    static const QChar delimiter() { return L'\n'; }
};

} // namespace event
} // namespace vms
} // namespace nx
