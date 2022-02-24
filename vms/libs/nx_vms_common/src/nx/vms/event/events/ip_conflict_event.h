// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/event/events/conflict_event.h>
#include <core/resource/resource_fwd.h>

class QHostAddress;
class QStringList;

namespace nx {
namespace vms {
namespace event {

class NX_VMS_COMMON_API IpConflictEvent: public ConflictEvent
{
    using base_type = ConflictEvent;

public:
    IpConflictEvent(const QnResourcePtr& resource, const QHostAddress& address,
        const QStringList& macAddrList,  qint64 timeStamp);

    static const QChar delimiter() { return '\n'; }
};

} // namespace event
} // namespace vms
} // namespace nx
