// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "ip_conflict_event.h"

#include <QtCore/QStringList>
#include <QtNetwork/QHostAddress>

namespace nx::vms::event {

IpConflictEvent::IpConflictEvent(
    const QnResourcePtr& resource,
    const QHostAddress& address,
    const QStringList& macAddrList,
    qint64 timeStamp,
    const QStringList& cameraRefs)
    :
    base_type(
        EventType::cameraIpConflictEvent,
        resource,
        timeStamp,
        address.toString(),
        macAddrList.join(delimiter())),
    m_cameraRefs(cameraRefs)
{
}

EventParameters IpConflictEvent::getRuntimeParams() const
{
    auto params = base_type::getRuntimeParams();
    params.metadata.cameraRefs.assign(m_cameraRefs.begin(), m_cameraRefs.end());

    return params;
}

} // namespace nx::vms::event
