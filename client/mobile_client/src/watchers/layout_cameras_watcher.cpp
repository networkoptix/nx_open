#include "layout_cameras_watcher.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource/layout_resource.h>
#include <core/resource/camera_resource.h>

namespace {
QnVirtualCameraResourcePtr getCamera(const QnLayoutItemData& item)
{
    return qnResPool->getResourceById<QnVirtualCameraResource>(item.resource.id);
}

} // namespace

namespace nx {
namespace client {
namespace mobile {

LayoutCamerasWatcher::LayoutCamerasWatcher(QObject* parent):
    base_type(parent)
{
    connect(this, &LayoutCamerasWatcher::cameraAdded, this, &LayoutCamerasWatcher::countChanged);
    connect(this, &LayoutCamerasWatcher::cameraRemoved, this, &LayoutCamerasWatcher::countChanged);
}

QnLayoutResourcePtr LayoutCamerasWatcher::layout() const
{
    return m_layout;
}

void LayoutCamerasWatcher::setLayout(const QnLayoutResourcePtr& layout)
{
    if (m_layout == layout)
        return;

    if (m_layout)
    {
        m_layout->disconnect(this);

        const auto cameras = m_cameras;
        m_cameras.clear();
        m_countByCameraId.clear();
        for (const auto& camera: cameras)
            emit cameraRemoved(camera);
    }

    m_layout = layout;

    emit layoutChanged(layout);
    emit countChanged();

    if (!layout)
        return;

    const auto itemAddedHandler =
        [this](const QnLayoutResourcePtr&, const QnLayoutItemData& item)
        {
            if (const auto camera = getCamera(item))
                addCamera(camera);
        };

    const auto itemRemovedHandler =
        [this](const QnLayoutResourcePtr&, const QnLayoutItemData& item)
        {
            if (getCamera(item))
                removeCamera(item.resource.id);
        };

    static const auto kNullLayoutResource = QnLayoutResourcePtr();
    for (const auto& item: layout->getItems())
        itemAddedHandler(kNullLayoutResource, item);

    connect(layout, &QnLayoutResource::itemAdded, this, itemAddedHandler);
    connect(layout, &QnLayoutResource::itemRemoved, this, itemRemovedHandler);

    connect(layout, &QnLayoutResource::itemChanged, this,
        [this, itemAddedHandler, itemRemovedHandler](const QnLayoutResourcePtr& /* resource */,
            const QnLayoutItemData& item,
            const QnLayoutItemData& oldItem)
        {
            itemRemovedHandler(kNullLayoutResource, oldItem);
            itemAddedHandler(kNullLayoutResource, item);
        });
}

QHash<QnUuid, QnVirtualCameraResourcePtr> LayoutCamerasWatcher::cameras() const
{
    return m_cameras;
}

int LayoutCamerasWatcher::count() const
{
    return m_cameras.size();
}

void LayoutCamerasWatcher::addCamera(const QnVirtualCameraResourcePtr& camera)
{
    const auto cameraId = camera->getId();

    m_cameras[cameraId] = camera;
    if (m_countByCameraId.insert(cameraId))
        emit cameraAdded(camera);
}

void LayoutCamerasWatcher::removeCamera(const QnUuid& cameraId)
{
    if (m_countByCameraId.remove(cameraId))
    {
        const auto camera = m_cameras.take(cameraId);
        NX_ASSERT(camera);
        if (camera)
            emit cameraRemoved(camera);
    }
}

} // namespace mobile
} // namespace client
} // namespace nx
