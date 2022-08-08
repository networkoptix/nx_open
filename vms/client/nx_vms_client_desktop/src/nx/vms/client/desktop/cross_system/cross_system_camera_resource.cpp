// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "cross_system_camera_resource.h"

#include <nx/vms/client/desktop/resources/resource_descriptor.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/common/system_context.h>

#include "cloud_cross_system_context.h"

namespace nx::vms::client::desktop {

namespace {

const QnUuid kThumbCameraTypeId("{72d232d7-0c67-4d8e-b5a8-a0d5075ff3a4}");

} // namespace

struct CrossSystemCameraResource::Private
{
    CrossSystemCameraResource* const q;
    nx::vms::api::CameraDataEx source;
    QPointer<CloudCrossSystemContext> crossSystemContext;
    CloudCrossSystemContext::Status cachedStatus = CloudCrossSystemContext::Status::uninitialized;

    QString calculateName() const
    {
        return crossSystemContext->isConnected()
            ? source.name
            : toString(cachedStatus);
    }

    api::ResourceStatus calculateResourceStatus() const
    {
        switch (cachedStatus)
        {
            case CloudCrossSystemContext::Status::uninitialized:
                return api::ResourceStatus::offline;
            case CloudCrossSystemContext::Status::connecting:
                return api::ResourceStatus::undefined;
            case CloudCrossSystemContext::Status::connectionFailure:
                return api::ResourceStatus::unauthorized;
            case CloudCrossSystemContext::Status::unsupported:
                return api::ResourceStatus::incompatible; //< Debug purposes.
            case CloudCrossSystemContext::Status::connected:
                return api::ResourceStatus::online; //< Debug purposes.
            default:
                return api::ResourceStatus::undefined;
        }
    }
};

CrossSystemCameraResource::CrossSystemCameraResource(
    CloudCrossSystemContext* crossSystemContext,
    const nx::vms::api::CameraDataEx& source)
    :
    QnClientCameraResource(source.typeId),
    d(new Private{
        .q = this,
        .source = source,
        .crossSystemContext = crossSystemContext
    })
{
    addFlags(Qn::cross_system);
    setIdUnsafe(d->source.id);
    setTypeId(d->source.typeId);
    setModel(d->source.model);
    setVendor(d->source.vendor);
    setPhysicalId(d->source.physicalId);
    setMAC(nx::utils::MacAddress(d->source.mac));

    m_name = d->source.name; //< Set resource name, but not camera name.
    m_url = d->source.url;
    m_parentId = d->source.parentId;

    // Test if the camera is a desktop or virtual camera.
    if (d->source.typeId == nx::vms::api::CameraData::kDesktopCameraTypeId)
        addFlags(Qn::desktop_camera);

    if (d->source.typeId == nx::vms::api::CameraData::kVirtualCameraTypeId)
        addFlags(Qn::virtual_camera);

    watchOnCrossSystemContext();
}

CrossSystemCameraResource::CrossSystemCameraResource(
    CloudCrossSystemContext* crossSystemContext,
    const nx::vms::common::ResourceDescriptor& descriptor)
    :
    QnClientCameraResource(kThumbCameraTypeId),
    d(new Private{
        .q = this,
        .crossSystemContext = crossSystemContext,
        .cachedStatus = crossSystemContext->status()
    })
{
    setIdUnsafe(descriptor.id);
    addFlags(Qn::cross_system | Qn::fake);

    m_name = d->calculateName(); //< Set resource name, but not camera name.

    watchOnCrossSystemContext();
}

CrossSystemCameraResource::~CrossSystemCameraResource() = default;

void CrossSystemCameraResource::update(nx::vms::api::CameraDataEx data)
{
    if (data.typeId != d->source.typeId)
    {
        // Test if the camera is a desktop or virtual camera.
        if (d->source.typeId == nx::vms::api::CameraData::kDesktopCameraTypeId)
            addFlags(Qn::desktop_camera);

        if (d->source.typeId == nx::vms::api::CameraData::kVirtualCameraTypeId)
            addFlags(Qn::virtual_camera);

        setTypeId(data.typeId);
        setModel(d->source.model);
        setVendor(d->source.vendor);
        setPhysicalId(d->source.physicalId);
        setMAC(nx::utils::MacAddress(d->source.mac));
    }

    NotifierList notifiers;
    {
        NX_MUTEX_LOCKER locker(&m_mutex);

        d->source = std::move(data);

        if (m_parentId != d->source.parentId)
        {
            const auto oldParentId = m_parentId;
            m_parentId = d->source.parentId;
            notifiers <<
                [r = toSharedPointer(this), oldParentId]
                {
                    emit r->parentIdChanged(r, oldParentId);
                };
        }

        if (m_url != d->source.url)
        {
            m_url = d->source.url;
            notifiers << [r = toSharedPointer(this)] { emit r->urlChanged(r); };
        }

        if (m_name != d->source.name)
        {
            m_name = d->source.name;
            notifiers << [r = toSharedPointer(this)] { emit r->nameChanged(r); };
        }

        notifiers <<
            [r = toSharedPointer(this)] { emit r->statusChanged(r, Qn::StatusChangeReason::Local); };
    }

    for (const auto& notifier: notifiers)
        notifier();
}

api::ResourceStatus CrossSystemCameraResource::getStatus() const
{
    // Returns resource status only is the system contains it is connected, otherwise calculate
    // status from the system status.
    return d->crossSystemContext->isConnected()
        ? QnClientCameraResource::getStatus()
        : d->calculateResourceStatus();
}

CloudCrossSystemContext* CrossSystemCameraResource::crossSystemContext() const
{
    return d->crossSystemContext;
}

nx::vms::common::ResourceDescriptor CrossSystemCameraResource::descriptor() const
{
    return nx::vms::client::desktop::descriptor(getId(), d->crossSystemContext->systemId());
}

void CrossSystemCameraResource::watchOnCrossSystemContext()
{
    const auto update =
        [this]
        {
            const auto newStatus = d->crossSystemContext->status();
            if (d->cachedStatus == newStatus)
                return;

            d->cachedStatus = newStatus;

            QnResource::setName(d->calculateName());
            if (d->crossSystemContext->isConnected())
                removeFlags(Qn::fake);
            else
                addFlags(Qn::fake);

            statusChanged(toSharedPointer(), Qn::StatusChangeReason::Local);
        };

    connect(
        d->crossSystemContext,
        &CloudCrossSystemContext::statusChanged,
        this,
        update);
}

} // namespace nx::vms::client::desktop
