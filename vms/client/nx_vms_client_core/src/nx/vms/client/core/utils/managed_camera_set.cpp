// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "managed_camera_set.h"

#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/log/log.h>
#include <nx/utils/qt_helpers.h>

namespace nx::vms::client::core {

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
        ? nx::utils::toQSet(
            m_resourcePool->getAllCameras(/*parentId*/ QnUuid(), /*ignoreDesktopCameras*/ true))
        : QnVirtualCameraResourceSet();

    setCameras(Type::all, filteredCameras());
}

void ManagedCameraSet::setSingleCamera(const QnVirtualCameraResourcePtr& camera)
{
    // Cameras from other cloud systems should be filtered out.
    m_notFilteredCameras = camera && cameraBelongsToLocalResourcePool(camera)
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
        // Cameras from other cloud systems should be filtered out.
        if (!cameraBelongsToLocalResourcePool(camera))
            invalid.insert(camera);
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

    emit camerasAboutToBeChanged(QPrivateSignal());

    m_type = type;
    m_cameras = cameras;

    emit camerasChanged(QPrivateSignal());
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

bool ManagedCameraSet::cameraBelongsToLocalResourcePool(const QnVirtualCameraResourcePtr& camera)
{
    return NX_ASSERT(camera)
        && NX_ASSERT(m_resourcePool)
        && camera->resourcePool() == m_resourcePool;
}

void ManagedCameraSet::addCamera(const QnVirtualCameraResourcePtr& camera)
{
    if (!camera || camera->hasFlags(Qn::desktop_camera))
        return;

    m_notFilteredCameras.insert(camera);
    if ((m_filter && !m_filter(camera)) || m_cameras.contains(camera))
        return;

    NX_VERBOSE(this, "Applicable camera added to the pool: %1", camera->getName());

    emit camerasAboutToBeChanged(QPrivateSignal());

    m_cameras.insert(camera);
    emit cameraAdded(camera, QPrivateSignal());

    emit camerasChanged(QPrivateSignal());
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

    emit camerasAboutToBeChanged(QPrivateSignal());

    m_notFilteredCameras.remove(camera);
    m_cameras.remove(camera);
    emit cameraRemoved(camera, QPrivateSignal());

    emit camerasChanged(QPrivateSignal());
}

ManagedCameraSet::Filter ManagedCameraSet::filter() const
{
    return m_filter;
}

} // namespace nx::vms::client::core
