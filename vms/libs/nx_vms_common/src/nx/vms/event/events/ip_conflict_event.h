// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/resource_fwd.h>
#include <nx/vms/event/events/conflict_event.h>

class QHostAddress;

namespace nx::vms::event {

class NX_VMS_COMMON_API IpConflictEvent: public ConflictEvent
{
    using base_type = ConflictEvent;

public:
    IpConflictEvent(
        const QnResourcePtr& resource,
        const QHostAddress& address,
        const QStringList& macAddrList,
        qint64 timeStamp,
        const QStringList& cameraRefs);

    virtual EventParameters getRuntimeParams() const override;

    static QString encodeMacAddrList(const QStringList& macAddrList);
    static QStringList decodeMacAddrList(const EventParameters& params);

private:
    QStringList m_cameraRefs;
};

} // namespace nx::vms::event
