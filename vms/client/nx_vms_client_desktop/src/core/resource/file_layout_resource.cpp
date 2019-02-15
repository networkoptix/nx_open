#include "file_layout_resource.h"

#include <nx/utils/log/log.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/avi/avi_resource.h>
#include <core/resource/camera_resource.h>

QnFileLayoutResource::QnFileLayoutResource(QnCommonModule* commonModule):
    base_type(commonModule),
    m_localStatus(Qn::Online)
{
    addFlags(Qn::exported_layout);
}

QString QnFileLayoutResource::getUniqueId() const
{
    return getUrl();
}

Qn::ResourceStatus QnFileLayoutResource::getStatus() const
{
    return m_localStatus;
}

bool QnFileLayoutResource::isFile() const
{
    NX_ASSERT(!getUrl().isEmpty());
    return true;
}

void QnFileLayoutResource::setStatus(Qn::ResourceStatus newStatus,
    Qn::StatusChangeReason reason)
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
