// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "cross_system_camera_resource.h"

#include <nx/vms/client/core/system_finder/system_description.h>

namespace nx::vms::client::core {

namespace {

const nx::Uuid kThumbCameraTypeId("{72d232d7-0c67-4d8e-b5a8-a0d5075ff3a4}");

/** Flags of the desktop and virtual cameras, which are defined by their types. */
Qn::ResourceFlags typeFlags(nx::Uuid typeId)
{
    if (typeId == nx::vms::api::CameraData::kDesktopCameraTypeId)
        return Qn::desktop_camera;

    if (typeId == nx::vms::api::CameraData::kVirtualCameraTypeId)
        return Qn::virtual_camera;

    return {};
}

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
    base_type(source.typeId),
    d(new Private{
        .q = this,
        .systemId = systemId,
        .source = source
    })
{
    addFlags(Qn::cross_system | typeFlags(source.typeId));
    setIdUnsafe(source.id);
    setTypeId(source.typeId);
    setModel(source.model);
    setVendor(source.vendor);
    setPhysicalId(source.physicalId);
    setMAC(nx::utils::MacAddress(source.mac));
    setGroupId(source.groupId);
    setDefaultGroupName(source.groupName);
    setScheduleEnabled(source.scheduleEnabled);
    setUserAttributes(source);

    m_name = source.name; //< Set resource name, but not camera name.
    m_url = source.url;
    m_parentId = source.parentId;
}

CrossSystemCameraResource::CrossSystemCameraResource(
    const QString& systemId, nx::Uuid id, const QString& name, nx::Uuid typeId):
    base_type(typeId.isNull() ? kThumbCameraTypeId : typeId),
    d(new Private{.q = this, .systemId = systemId})
{
    setIdUnsafe(id);
    m_name = name.isEmpty() //< Set resource name, but not camera name.
        ? tr("Unknown camera") //< Actually is a fallback and should never be seen.
        : name;
    addFlags(Qn::cross_system | Qn::fake | typeFlags(typeId));
}

CrossSystemCameraResource::~CrossSystemCameraResource()
{
    // Required here for forward-declared scoped pointer destruction.
}

void CrossSystemCameraResource::update(nx::vms::api::CameraDataEx data)
{
    if (!d->source)
    {
        addFlags(typeFlags(data.typeId));
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

    // Unlike the type, model and other fields set only on the first update, the group may change.
    setGroupId(d->source->groupId);
    setDefaultGroupName(d->source->groupName);
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

} // namespace nx::vms::client::core
