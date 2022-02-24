// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "ip_conflict_event.h"

#include <QtCore/QStringList>
#include <QtNetwork/QHostAddress>

namespace nx {
namespace vms {
namespace event {

IpConflictEvent::IpConflictEvent(
    const QnResourcePtr& resource,
    const QHostAddress& address,
    const QStringList& macAddrList,
    qint64 timeStamp)
    :
    base_type(
        EventType::cameraIpConflictEvent, resource, timeStamp,
        address.toString(), macAddrList.join(delimiter()))
{
}

} // namespace event
} // namespace vms
} // namespace nx
