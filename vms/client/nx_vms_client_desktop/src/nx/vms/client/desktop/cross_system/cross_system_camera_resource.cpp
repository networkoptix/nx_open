// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "cross_system_camera_resource.h"

#include <nx/vms/common/system_context.h>
#include <core/resource_management/status_dictionary.h>

namespace nx::vms::client::desktop {

CrossSystemCameraResource::CrossSystemCameraResource(nx::vms::api::CameraDataEx source):
    QnClientCameraResource(source.typeId),
    m_source(std::move(source))
{
    addFlags(Qn::cross_system);
    setIdUnsafe(m_source.id);
    setTypeId(m_source.typeId);
    setModel(m_source.model);
    setVendor(m_source.vendor);
    setPhysicalId(m_source.physicalId);
    setMAC(nx::utils::MacAddress(m_source.mac));
    setParentId(m_source.parentId);

    QnResource::setName(m_source.name); //< Set resource name, but not camera name.
    setUrl(m_source.url);

    // Test if the camera is a desktop or virtual camera.
    if (m_source.typeId == nx::vms::api::CameraData::kDesktopCameraTypeId)
        addFlags(Qn::desktop_camera);

    if (m_source.typeId == nx::vms::api::CameraData::kVirtualCameraTypeId)
        addFlags(Qn::virtual_camera);
}

void CrossSystemCameraResource::update(nx::vms::api::CameraDataEx data)
{
    NotifierList notifiers;
    {
        NX_MUTEX_LOCKER locker(&m_mutex);
        m_source = std::move(data);

        if (m_parentId != m_source.parentId)
        {
            const auto oldParentId = m_parentId;
            m_parentId = m_source.parentId;
            notifiers <<
                [r = toSharedPointer(this), oldParentId]
                {
                    emit r->parentIdChanged(r, oldParentId);
                };
        }

        if (m_url != m_source.url)
        {
            m_url = m_source.url;
            notifiers << [r = toSharedPointer(this)] { emit r->urlChanged(r); };
        }

        if (m_name != m_source.name)
        {
            m_name = m_source.name;
            notifiers << [r = toSharedPointer(this)] { emit r->nameChanged(r); };
        }
    }

    for (const auto& notifier: notifiers)
        notifier();
}

} // namespace nx::vms::client::desktop
