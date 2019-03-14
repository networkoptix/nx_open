#include "managed_camera_set.h"

#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>

#include <nx/utils/log/assert.h>
#include <nx/utils/log/log.h>

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

QnVirtualCameraResourceSet ManagedCameraSet::allCameras() const
{
    if (!m_resourcePool)
        return {};

    return m_filter
        ? m_resourcePool->getResources<QnVirtualCameraResource>(m_filter).toSet()
        : m_resourcePool->getResources<QnVirtualCameraResource>().toSet();
}

void ManagedCameraSet::setAllCameras()
{
    if (m_type == Type::all)
        return;

    NX_ASSERT(m_resourcePool);
    setCameras(Type::all, allCameras());
}

void ManagedCameraSet::setSingleCamera(const QnVirtualCameraResourcePtr& camera)
{
    if (!camera || (m_resourcePool && camera->resourcePool() != m_resourcePool)
        || (m_filter && !m_filter(camera)))
    {
        setCameras(Type::single, {});
    }
    else
    {
        setCameras(Type::single, {camera});
    }
}

void ManagedCameraSet::setMultipleCameras(const QnVirtualCameraResourceSet& cameras)
{
    QnVirtualCameraResourceSet filteredCameras;
    for (const auto& camera: cameras)
    {
        if ((!m_resourcePool || camera->resourcePool() == m_resourcePool)
            && (!m_filter || m_filter(camera)))
        {
            filteredCameras.insert(camera);
        }
    }

    setCameras(Type::multiple, filteredCameras);
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

void ManagedCameraSet::addCamera(const QnVirtualCameraResourcePtr& camera)
{
    if (!camera || (m_filter && !m_filter(camera)) || m_cameras.contains(camera))
        return;

    emit camerasAboutToBeChanged({});

    m_cameras.insert(camera);
    emit cameraAdded(camera, {});

    emit camerasChanged({});
}

void ManagedCameraSet::removeCamera(const QnVirtualCameraResourcePtr& camera)
{
    if (!camera || !m_cameras.contains(camera))
        return;

    NX_VERBOSE(this) << "Camera removed from the pool:" << camera->getName();

    emit camerasAboutToBeChanged({});

    m_cameras.remove(camera);
    emit cameraRemoved(camera, {});

    emit camerasChanged({});
}

ManagedCameraSet::Filter ManagedCameraSet::filter() const
{
    return m_filter;
}

} // namespace nx::vms::client::desktop
