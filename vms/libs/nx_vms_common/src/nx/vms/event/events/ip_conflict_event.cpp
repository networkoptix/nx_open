// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "ip_conflict_event.h"

#include <QtCore/QStringList>
#include <QtNetwork/QHostAddress>

namespace nx::vms::event {

namespace {

constexpr auto kDelimiter = QChar{'\n'};

} // namespace

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
        address.toString(), //< Caption.
        encodeMacAddrList(macAddrList)), //< Description.
    m_cameraRefs(cameraRefs)
{
}

EventParameters IpConflictEvent::getRuntimeParams() const
{
    auto params = base_type::getRuntimeParams();
    params.metadata.cameraRefs.assign(m_cameraRefs.begin(), m_cameraRefs.end());

    return params;
}

QString IpConflictEvent::encodeMacAddrList(const QStringList& macAddrList)
{
    return macAddrList.join(kDelimiter);
}


QStringList IpConflictEvent::decodeMacAddrList(const EventParameters& params)
{
    return params.description.split(kDelimiter);
}

} // namespace nx::vms::event
