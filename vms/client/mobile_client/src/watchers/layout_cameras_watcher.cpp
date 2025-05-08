// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "layout_cameras_watcher.h"

#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/core/resource/resource_descriptor_helpers.h>

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

    const auto handleItemAdded =
        [this](const QnLayoutResourcePtr& layout, const nx::vms::common::LayoutItemData& item)
        {
            if (const auto camera = nx::vms::client::core::getResourceByDescriptor(
                item.resource).dynamicCast<QnVirtualCameraResource>())
            {
                addCamera(camera);
            }
        };

    const auto handleItemRemoved =
        [this](const QnLayoutResourcePtr&, const nx::vms::common::LayoutItemData& item)
        {
            removeCamera(item.resource.id);
        };

    for (const auto& item: layout->getItems())
        handleItemAdded(layout, item);

    connect(layout.get(), &QnLayoutResource::itemAdded, this, handleItemAdded);
    connect(layout.get(), &QnLayoutResource::itemRemoved, this, handleItemRemoved);

    connect(layout.get(), &QnLayoutResource::itemChanged, this,
        [=](const QnLayoutResourcePtr&,
            const nx::vms::common::LayoutItemData& item,
            const nx::vms::common::LayoutItemData& oldItem)
        {
            if (item.resource == oldItem.resource)
                return;

            handleItemRemoved(layout, oldItem);
            handleItemAdded(layout, item);
        });
}

QHash<nx::Uuid, QnVirtualCameraResourcePtr> LayoutCamerasWatcher::cameras() const
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

void LayoutCamerasWatcher::removeCamera(const nx::Uuid& cameraId)
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
