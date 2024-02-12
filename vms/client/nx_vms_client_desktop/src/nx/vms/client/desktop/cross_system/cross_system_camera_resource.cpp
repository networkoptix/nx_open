// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "cross_system_camera_resource.h"

#include <network/base_system_description.h>
#include <nx/vms/client/desktop/resource/resource_descriptor.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/common/system_context.h>

#include "cloud_cross_system_context.h"

namespace nx::vms::client::desktop {

namespace {

const nx::Uuid kThumbCameraTypeId("{72d232d7-0c67-4d8e-b5a8-a0d5075ff3a4}");

} // namespace

struct CrossSystemCameraResource::Private
{
    CrossSystemCameraResource* const q;
    const QString systemId;
    std::optional<nx::vms::api::CameraDataEx> source;
};

CrossSystemCameraResource::CrossSystemCameraResource(
    const QString& systemId,
    const nx::vms::api::CameraDataEx& source)
    :
    QnClientCameraResource(source.typeId),
    d(new Private{
        .q = this,
        .systemId = systemId,
        .source = source
    })
{
    addFlags(Qn::cross_system);
    setIdUnsafe(source.id);
    setTypeId(source.typeId);
    setModel(source.model);
    setVendor(source.vendor);
    setPhysicalId(source.physicalId);
    setMAC(nx::utils::MacAddress(source.mac));
    setScheduleEnabled(source.scheduleEnabled);
    setUserAttributes(source);

    m_name = source.name; //< Set resource name, but not camera name.
    m_url = source.url;
    m_parentId = source.parentId;

    // Test if the camera is a desktop or virtual camera.
    if (source.typeId == nx::vms::api::CameraData::kDesktopCameraTypeId)
        addFlags(Qn::desktop_camera);

    if (source.typeId == nx::vms::api::CameraData::kVirtualCameraTypeId)
        addFlags(Qn::virtual_camera);
}

CrossSystemCameraResource::CrossSystemCameraResource(
    const QString& systemId,
    const nx::Uuid& id,
    const QString& name)
    :
    QnClientCameraResource(kThumbCameraTypeId),
    d(new Private{
        .q = this,
        .systemId = systemId
    })
{
    setIdUnsafe(id);
    m_name = name.isEmpty() //< Set resource name, but not camera name.
        ? tr("Unknown camera") //< Actually is a fallback and should never be seen.
        : name;
    addFlags(Qn::cross_system | Qn::fake);
}

CrossSystemCameraResource::~CrossSystemCameraResource() = default;

void CrossSystemCameraResource::update(nx::vms::api::CameraDataEx data)
{
    if (!d->source)
    {
        // Test if the camera is a desktop or virtual camera.
        if (data.typeId == nx::vms::api::CameraData::kDesktopCameraTypeId)
            addFlags(Qn::desktop_camera);

        if (data.typeId == nx::vms::api::CameraData::kVirtualCameraTypeId)
            addFlags(Qn::virtual_camera);

        removeFlags(Qn::fake);

        setTypeId(data.typeId);
        setModel(data.model);
        setVendor(data.vendor);
        setPhysicalId(data.physicalId);
        setMAC(nx::utils::MacAddress(data.mac));
    }

    NotifierList notifiers;
    {
        NX_MUTEX_LOCKER locker(&m_mutex);

        d->source = std::move(data);

        if (m_parentId != d->source->parentId)
        {
            const auto oldParentId = m_parentId;
            m_parentId = d->source->parentId;
            notifiers <<
                [r = toSharedPointer(this), oldParentId]
                {
                    emit r->parentIdChanged(r, oldParentId);
                };
        }

        if (m_url != d->source->url)
        {
            m_url = d->source->url;
            notifiers << [r = toSharedPointer(this)] { emit r->urlChanged(r); };
        }

        if (m_name != d->source->name)
        {
            m_name = d->source->name;
            notifiers << [r = toSharedPointer(this)] { emit r->nameChanged(r); };
        }

        notifiers <<
            [r = toSharedPointer(this)] { emit r->statusChanged(r, Qn::StatusChangeReason::Local); };
    }

    for (const auto& notifier: notifiers)
        notifier();

    setScheduleEnabled(d->source->scheduleEnabled);
    setUserAttributesAndNotify(*d->source);
}

QString CrossSystemCameraResource::systemId() const
{
    return d->systemId;
}

const std::optional<api::CameraDataEx>& CrossSystemCameraResource::source() const
{
    return d->source;
}

} // namespace nx::vms::client::desktop
