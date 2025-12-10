// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "layout_cameras_watcher.h"

#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/range_adapters.h>
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

        for (const auto& camera: nx::utils::keyRange(cameras))
            emit cameraRemoved(camera);
    }

    m_layout = layout;

    emit layoutChanged(layout);
    emit countChanged();

    if (!layout)
        return;

    const auto handleItemAdded =
        [this](const QnLayoutResourcePtr& /*layout*/, const nx::vms::common::LayoutItemData& item)
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
            if (const auto camera = nx::vms::client::core::getResourceByDescriptor(
                item.resource).dynamicCast<QnVirtualCameraResource>())
            {
                removeCamera(camera);
            }
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

QnVirtualCameraResourceList LayoutCamerasWatcher::cameras() const
{
    return m_cameras.keys();
}

int LayoutCamerasWatcher::count() const
{
    return m_cameras.keyCount();
}

void LayoutCamerasWatcher::addCamera(const QnVirtualCameraResourcePtr& camera)
{
    if (!m_cameras.insert(camera))
        return;

    connect(camera.get(), &QnResource::flagsChanged, this,
        [this](const QnResourcePtr& resource)
        {
            const auto camera = resource.objectCast<QnVirtualCameraResource>();
            if (NX_ASSERT(camera) && camera->hasFlags(Qn::removed))
            {
                camera->disconnect(this);

                if (NX_ASSERT(m_cameras.removeAll(camera) > 0))
                    emit cameraRemoved(camera);
            }
        });

    emit cameraAdded(camera);
}

void LayoutCamerasWatcher::removeCamera(const QnVirtualCameraResourcePtr& camera)
{
    if (!m_cameras.remove(camera))
        return;

    camera->disconnect(this);
    emit cameraRemoved(camera);
}

} // namespace mobile
} // namespace client
} // namespace nx
