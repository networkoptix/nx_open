// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "abstract_archive_resource.h"
#include <nx/utils/log/log.h>

QnAbstractArchiveResource::QnAbstractArchiveResource()
{
    QnMediaResource::initMediaResource();

    addFlags(Qn::local_video);
    m_localStatus = nx::vms::api::ResourceStatus::online;
}

QnAbstractArchiveResource::~QnAbstractArchiveResource()
{
}

nx::vms::api::ResourceStatus QnAbstractArchiveResource::getStatus() const
{
    return m_localStatus;
}

void QnAbstractArchiveResource::setStatus(nx::vms::api::ResourceStatus newStatus, Qn::StatusChangeReason reason)
{
    if (m_localStatus == newStatus)
        return;

    m_localStatus = newStatus;

    // Null pointer if we are changing status in constructor. Signal is not needed in this case.
    if (auto sharedThis = toSharedPointer(this))
    {
        NX_VERBOSE(this, "Signal status change for %1", newStatus);
        emit statusChanged(sharedThis, reason);
    }
}
