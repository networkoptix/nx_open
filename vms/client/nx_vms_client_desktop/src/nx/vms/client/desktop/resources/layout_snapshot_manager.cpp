// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "layout_snapshot_manager.h"

#include <core/resource/layout_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/guarded_callback.h>
#include <nx/vms/api/data/layout_data.h>
#include <nx/vms/client/core/network/remote_connection.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/cross_system/cloud_layouts_manager.h>
#include <nx/vms/client/desktop/cross_system/cross_system_layout_resource.h>
#include <nx/vms/client/desktop/resources/resource_descriptor.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx_ec/abstract_ec_connection.h>
#include <nx_ec/data/api_conversion_functions.h>
#include <nx_ec/managers/abstract_layout_manager.h>
#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_layout_synchronizer.h>

#include "layout_snapshot_storage.h"

namespace nx::vms::client::desktop {

LayoutSnapshotManager::LayoutSnapshotManager(SystemContext* systemContext, QObject* parent):
    base_type(parent),
    SystemContextAware(systemContext),
    m_storage(new LayoutSnapshotStorage(this))
{
    // Start listening to changes.
    connect(resourcePool(), &QnResourcePool::resourcesAdded, this,
        &LayoutSnapshotManager::onResourcesAdded);
    connect(resourcePool(), &QnResourcePool::resourcesRemoved, this,
        &LayoutSnapshotManager::onResourcesRemoved);

    onResourcesAdded(resourcePool()->getResources<QnLayoutResource>());

    connect(this, &QnAbstractSaveStateManager::flagsChanged, this,
        [this](const QnUuid& id, SaveStateFlags /*flags*/)
        {
            if (const auto layout = resourcePool()->getResourceById<QnLayoutResource>(id))
                emit layoutFlagsChanged(layout);
        });
}

LayoutSnapshotManager::~LayoutSnapshotManager()
{
}

QnAbstractSaveStateManager::SaveStateFlags LayoutSnapshotManager::flags(
    const QnLayoutResourcePtr& layout) const
{
    NX_ASSERT(layout);
    if (!layout)
        return SaveStateFlags();
    return base_type::flags(layout->getId());
}

QnAbstractSaveStateManager::SaveStateFlags LayoutSnapshotManager::flags(
    QnWorkbenchLayout* layout) const
{
    NX_ASSERT(layout); //< Layout resource can be null.
    if (!layout || !layout->resource())
        return SaveStateFlags();

    return flags(layout->resource());
}

void LayoutSnapshotManager::setFlags(const QnLayoutResourcePtr& layout,
    SaveStateFlags flags)
{
    NX_ASSERT(layout && layout->resourcePool(),
        "Could not set flags to resource which is not in pool");

    if (!layout)
        return;

    base_type::setFlags(layout->getId(), flags);
}

bool LayoutSnapshotManager::isChanged(const QnLayoutResourcePtr &layout) const
{
    return base_type::isChanged(layout->getId());
}

bool LayoutSnapshotManager::isSaveable(const QnLayoutResourcePtr &layout) const
{
    if (layout->hasFlags(Qn::local))
        return true;

    return base_type::isSaveable(layout->getId());
}

bool LayoutSnapshotManager::isModified(const QnLayoutResourcePtr &layout) const
{
    /* Changed and not being saved. */
    return base_type::isSaveable(layout->getId());
}

bool LayoutSnapshotManager::save(
    const QnLayoutResourcePtr& layout,
    SaveLayoutResultFunction callback)
{
    NX_ASSERT(!layout->isFile());
    if (layout->isFile())
        return false;

    auto internalCallback = nx::utils::guarded(this,
        [this, layout, callback](bool success)
        {
            markBeingSaved(layout->getId(), false);

            if (success)
            {
                store(layout); //< Cleanup 'changed' flag here, sending corresponding signal.
                clean(layout->getId()); //< Silently remove from flags storage.
            }

            if (callback)
                callback(success, layout);
        });

    if (layout.dynamicCast<CrossSystemLayoutResource>())
    {
        appContext()->cloudLayoutsManager()->saveLayout(layout, internalCallback);
        return true;
    }

    auto connection = this->connection()->messageBusConnection();
    if (!NX_ASSERT(connection))
        return false;

    /* Submit all changes from workbench to resource. */
    if (QnWorkbenchLayoutSynchronizer *synchronizer = QnWorkbenchLayoutSynchronizer::instance(layout))
        synchronizer->submit();

    nx::vms::api::LayoutData apiLayout;
    ec2::fromResourceToApi(layout, apiLayout);

    int reqID = connection->getLayoutManager(Qn::kSystemAccess)->save(
        apiLayout,
        [this, internalCallback](int /*reqID*/, ec2::ErrorCode errorCode)
        {
            const bool success = errorCode == ec2::ErrorCode::ok;
            internalCallback(success);
        },
        this);
    bool success = reqID > 0;
    if (success)
        markBeingSaved(layout->getId(), true);
    return success;
}

const nx::vms::api::LayoutData& LayoutSnapshotManager::snapshot(
    const QnLayoutResourcePtr& layout) const
{
    static nx::vms::api::LayoutData emptyResult;

    if (!NX_ASSERT(layout))
        return emptyResult;
    return m_storage->snapshot(layout);
}

void LayoutSnapshotManager::store(const QnLayoutResourcePtr &resource)
{
    if (!NX_ASSERT(resource))
        return;

    /* We don't want to get queued layout change signals that are not yet delivered,
     * so there are no options but to disconnect. */
    disconnectFrom(resource);
    m_storage->store(resource);
    connectTo(resource);
    markChanged(resource->getId(), false);
}

void LayoutSnapshotManager::restore(const QnLayoutResourcePtr &resource)
{
    if (!NX_ASSERT(resource))
        return;

    /* We don't want to get queued layout change signals for these changes,
     * so there are no options but to disconnect before making them. */
    disconnectFrom(resource);
    {
        const nx::vms::api::LayoutData& snapshot = m_storage->snapshot(resource);
        QnLayoutResourcePtr restored(new QnLayoutResource());
        ec2::fromApiToResource(snapshot, restored);

        // Overwrite flags as they are runtime data and not stored in the snapshot.
        restored->setFlags(resource->flags());

        // Cleanup from snapshot resources which are already deleted from the resource pool.
        QnLayoutItemDataMap existingItems;
        for (const auto& item: restored->getItems())
        {
            if (!getResourceByDescriptor(item.resource))
                continue;

            existingItems.insert(item.uuid, item);
        }

        restored->setItems(existingItems);
        resource->update(restored);
    }
    connectTo(resource);

    if (auto synchronizer = QnWorkbenchLayoutSynchronizer::instance(resource))
        synchronizer->reset();

    markChanged(resource->getId(), false);
}

void LayoutSnapshotManager::connectTo(const QnLayoutResourcePtr &resource)
{
    connect(resource.get(), &QnResource::nameChanged, this,
        &LayoutSnapshotManager::at_resource_changed);
    connect(resource.get(), &QnResource::parentIdChanged, this,
        &LayoutSnapshotManager::at_resource_changed);
    connect(resource.get(), &QnResource::logicalIdChanged, this,
        &LayoutSnapshotManager::at_resource_changed);
    connect(resource.get(), &QnLayoutResource::itemAdded, this,
        &LayoutSnapshotManager::at_layout_changed);
    connect(resource.get(), &QnLayoutResource::itemRemoved, this,
        &LayoutSnapshotManager::at_layout_changed);
    connect(resource.get(), &QnLayoutResource::lockedChanged, this,
        &LayoutSnapshotManager::at_layout_changed);
    connect(resource.get(), &QnLayoutResource::cellAspectRatioChanged, this,
        &LayoutSnapshotManager::at_layout_changed);
    connect(resource.get(), &QnLayoutResource::cellSpacingChanged, this,
        &LayoutSnapshotManager::at_layout_changed);
    connect(resource.get(), &QnLayoutResource::backgroundImageChanged, this,
        &LayoutSnapshotManager::at_layout_changed);
    connect(resource.get(), &QnLayoutResource::backgroundSizeChanged, this,
        &LayoutSnapshotManager::at_layout_changed);
    connect(resource.get(), &QnLayoutResource::backgroundOpacityChanged, this,
        &LayoutSnapshotManager::at_layout_changed);
    connect(resource.get(), &QnLayoutResource::fixedSizeChanged, this,
        &LayoutSnapshotManager::at_layout_changed);

    // This one is handled separately because it should not lead to marking layout as modified if
    // layout is not opened right now.
    connect(resource.get(), &QnLayoutResource::itemChanged, this,
        &LayoutSnapshotManager::at_layout_changed);

    connect(resource.get(), &QnLayoutResource::storeRequested, this,
        &LayoutSnapshotManager::store);
}

void LayoutSnapshotManager::disconnectFrom(const QnLayoutResourcePtr &resource)
{
    resource->disconnect(this);
}

// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void LayoutSnapshotManager::onResourcesAdded(const QnResourceList& resources)
{
    for (const auto& layout: resources.filtered<QnLayoutResource>())
    {
        /* Consider it saved by default. */
        m_storage->store(layout);

        clean(layout->getId()); //< Not changed, not being saved.

        /* Subscribe to changes to track changed status. */
        connectTo(layout);
    }
}

void LayoutSnapshotManager::onResourcesRemoved(const QnResourceList& resources)
{
    for (const auto& layout: resources.filtered<QnLayoutResource>())
    {
        m_storage->remove(layout);
        clean(layout->getId());
        disconnectFrom(layout);
    }
}

void LayoutSnapshotManager::at_layout_changed(const QnLayoutResourcePtr& layout)
{
    markChanged(layout->getId(), true);
}

void LayoutSnapshotManager::at_resource_changed(const QnResourcePtr &resource)
{
    QnLayoutResourcePtr layoutResource = resource.dynamicCast<QnLayoutResource>();
    NX_ASSERT(layoutResource);
    if (layoutResource)
        at_layout_changed(layoutResource);
}

} // namespace nx::vms::client::desktop
