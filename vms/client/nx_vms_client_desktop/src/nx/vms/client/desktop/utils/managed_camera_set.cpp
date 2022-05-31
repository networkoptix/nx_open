// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "managed_camera_set.h"

#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>

#include <nx/utils/log/assert.h>
#include <nx/utils/log/log.h>
#include <nx/utils/qset.h>

namespace nx::vms::client::desktop {

ManagedCameraSet::ManagedCameraSet(QnResourcePool* resourcePool, Filter filter, QObject* parent):
    base_type(parent),
    m_resourcePool(resourcePool),
    m_filter(filter)
{
    if (!m_resourcePool)
        return;

    connect(m_resourcePool, &QnResourcePool::resourceAdded, this,
        [this](const QnResourcePtr& resource)
        {
            if (m_type == Type::all)
                addCamera(resource.dynamicCast<QnVirtualCameraResource>());
        });

    connect(m_resourcePool, &QnResourcePool::resourceRemoved, this,
        [this](const QnResourcePtr& resource)
        {
            removeCamera(resource.dynamicCast<QnVirtualCameraResource>());
        });
}

ManagedCameraSet::Type ManagedCameraSet::type() const
{
    return m_type;
}

QnVirtualCameraResourceSet ManagedCameraSet::cameras() const
{
    return m_cameras;
}

QnVirtualCameraResourcePtr ManagedCameraSet::singleCamera() const
{
    return m_type == Type::single && !m_cameras.empty()
        ? *m_cameras.cbegin()
        : QnVirtualCameraResourcePtr();
}

void ManagedCameraSet::setAllCameras()
{
    if (m_type == Type::all)
        return;

    m_notFilteredCameras = NX_ASSERT(m_resourcePool)
        ? nx::utils::toQSet(m_resourcePool->getResources<QnVirtualCameraResource>())
        : QnVirtualCameraResourceSet();

    if (objectName() == "TEST")
    {
        qDebug() << "Set cameras" << m_notFilteredCameras.size();
    }

    setCameras(Type::all, filteredCameras());
}

void ManagedCameraSet::setSingleCamera(const QnVirtualCameraResourcePtr& camera)
{
    m_notFilteredCameras = camera
        && NX_ASSERT(m_resourcePool && camera->resourcePool() == m_resourcePool)
            ? QnVirtualCameraResourceSet({camera})
            : QnVirtualCameraResourceSet();

    setCameras(Type::single, filteredCameras());
}

void ManagedCameraSet::setMultipleCameras(const QnVirtualCameraResourceSet& cameras)
{
    m_notFilteredCameras = cameras;

    QnVirtualCameraResourceSet invalid;
    for (const auto& camera: m_notFilteredCameras)
    {
        if (!(camera && m_resourcePool && camera->resourcePool() == m_resourcePool))
            invalid.insert(camera); // Cameras from other cloud systems can be here.
    }

    m_notFilteredCameras -= invalid;
    setCameras(Type::multiple, filteredCameras());
}

void ManagedCameraSet::invalidateFilter()
{
    if (!m_filter)
        return;

    NX_VERBOSE(this, "Filter invalidated");
    setCameras(m_type, filteredCameras());
}

void ManagedCameraSet::setCameras(Type type, const QnVirtualCameraResourceSet& cameras)
{
    if (m_type == type && m_cameras == cameras)
        return;

    emit camerasAboutToBeChanged({});

    m_type = type;
    m_cameras = cameras;

    emit camerasChanged({});
}

QnVirtualCameraResourceSet ManagedCameraSet::filteredCameras() const
{
    if (!m_filter)
        return m_notFilteredCameras;

    QnVirtualCameraResourceSet filtered;
    for (const auto& camera: m_notFilteredCameras)
    {
        if (m_filter(camera))
            filtered.insert(camera);
    }

    return filtered;
}

void ManagedCameraSet::addCamera(const QnVirtualCameraResourcePtr& camera)
{
    if (!camera)
        return;

    m_notFilteredCameras.insert(camera);
    if ((m_filter && !m_filter(camera)) || m_cameras.contains(camera))
        return;

    NX_VERBOSE(this, "Applicable camera added to the pool: %1", camera->getName());

    emit camerasAboutToBeChanged({});

    m_cameras.insert(camera);
    emit cameraAdded(camera, {});

    emit camerasChanged({});
}

void ManagedCameraSet::removeCamera(const QnVirtualCameraResourcePtr& camera)
{
    if (!camera)
        return;

    if (!m_cameras.contains(camera))
    {
        m_notFilteredCameras.remove(camera);
        return;
    }

    NX_VERBOSE(this, "Applicable camera removed from the pool: %1", camera->getName());

    emit camerasAboutToBeChanged({});

    m_notFilteredCameras.remove(camera);
    m_cameras.remove(camera);
    emit cameraRemoved(camera, {});

    emit camerasChanged({});
}

ManagedCameraSet::Filter ManagedCameraSet::filter() const
{
    return m_filter;
}

} // namespace nx::vms::client::desktop
