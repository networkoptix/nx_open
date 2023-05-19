// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "server.h"

#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>

namespace nx::vms::client::core {

void ServerResource::setStatus(
    nx::vms::api::ResourceStatus newStatus,
    Qn::StatusChangeReason reason)
{
    base_type::setStatus(newStatus, reason);
    if (auto resourcePool = this->resourcePool())
    {
        auto childCameras = resourcePool->getAllCameras(getId());
        for (const auto& camera: childCameras)
        {
            NX_VERBOSE(this, "%1 Emit statusChanged signal for resource %2", __func__, camera);
            emit camera->statusChanged(camera, Qn::StatusChangeReason::Local);
        }
    }
}

ServerResource::TimeZoneInfo ServerResource::timeZoneInfo() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_timeZoneInfo;
}

void ServerResource::setTimeZoneInfo(TimeZoneInfo value)
{
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        if (m_timeZoneInfo == value)
            return;

        m_timeZoneInfo = value;
    }
    emit timeZoneInfoChanged();
}

void ServerResource::updateInternal(const QnResourcePtr& source, NotifierList& notifiers)
{
    base_type::updateInternal(source, notifiers);

    auto localOther = dynamic_cast<ServerResource*>(source.data());
    if (NX_ASSERT(localOther))
        m_timeZoneInfo = localOther->m_timeZoneInfo;
}

} // namespace nx::vms::client::core
