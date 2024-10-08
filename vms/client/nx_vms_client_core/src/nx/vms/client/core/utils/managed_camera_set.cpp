// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "managed_camera_set.h"

#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/log/log.h>
#include <nx/utils/qt_helpers.h>

namespace nx::vms::client::core {

ManagedCameraSet::ManagedCameraSet(Filter filter, QObject* parent):
    base_type(parent),
    m_filter(filter)
{
}

ManagedCameraSet::~ManagedCameraSet()
{
}

void ManagedCameraSet::setResourcePool(QnResourcePool* resourcePool)
{
    NX_ASSERT(resourcePool);

    const bool camerasSetIsChanged = !m_cameras.empty();

    if (camerasSetIsChanged)
        emit camerasAboutToBeChanged(QPrivateSignal());

    if (m_resourcePool)
        m_resourcePool->disconnect(this);

    m_resourcePool = resourcePool;

    m_type = Type::multiple;
    m_cameras.clear();

    if (camerasSetIsChanged)
        emit camerasChanged(QPrivateSignal());

    invalidateFilter();
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

    if (m_resourcePool)
    {
        m_notFilteredCameras = NX_ASSERT(m_resourcePool)
            ? nx::utils::toQSet(
                m_resourcePool->getAllCameras(/*parentId*/ nx::Uuid(), /*ignoreDesktopCameras*/ true))
            : QnVirtualCameraResourceSet();

        setCameras(Type::all, filteredCameras());
    }
    else
    {
        m_type = Type::all;
    }
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
    if (!NX_ASSERT(m_resourcePool))
        return false;

    return NX_ASSERT(camera) && camera->resourcePool() == m_resourcePool;
}

void ManagedCameraSet::handleResourcesAdded(const QnResourceList& resources)
{
    if (m_type != Type::all)
        return;

    QnVirtualCameraResourceSet addedCameras;

    for (const auto& resource: resources)
    {
        const auto& camera = resource.dynamicCast<QnVirtualCameraResource>();
        if (!camera || camera->hasFlags(Qn::desktop_camera))
            continue;

        m_notFilteredCameras.insert(camera);
        if ((m_filter && !m_filter(camera)) || m_cameras.contains(camera))
            continue;

        NX_VERBOSE(this, "Applicable camera added to the pool: %1", camera->getName());
        addedCameras.insert(camera);
    }

    if (addedCameras.empty())
        return;

    emit camerasAboutToBeChanged(QPrivateSignal());

    m_cameras += addedCameras;

    emit camerasChanged(QPrivateSignal());
}

void ManagedCameraSet::handleResourcesRemoved(const QnResourceList& resources)
{
    QnVirtualCameraResourceSet removedCameras;

    for (const auto& resource: resources)
    {
        const auto& camera = resource.dynamicCast<QnVirtualCameraResource>();
        if (!camera)
            continue;

        if (m_cameras.contains(camera))
        {
            removedCameras.insert(camera);
            NX_VERBOSE(this, "Applicable camera removed from the pool: %1", camera->getName());
        }
        m_notFilteredCameras.remove(camera);
    }

    if (removedCameras.empty())
        return;

    emit camerasAboutToBeChanged(QPrivateSignal());

    m_cameras -= removedCameras;

    emit camerasChanged(QPrivateSignal());
}

ManagedCameraSet::Filter ManagedCameraSet::filter() const
{
    return m_filter;
}

} // namespace nx::vms::client::core
