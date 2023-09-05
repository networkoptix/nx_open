// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "layout_snapshot_manager.h"

#include <core/resource_management/resource_pool.h>
#include <nx/utils/guarded_callback.h>
#include <nx/vms/api/data/layout_data.h>
#include <nx/vms/client/core/network/remote_connection.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/cross_system/cloud_layouts_manager.h>
#include <nx/vms/client/desktop/cross_system/cross_system_layout_resource.h>
#include <nx/vms/client/desktop/resource/layout_resource.h>
#include <nx/vms/client/desktop/resource/resource_descriptor.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/workbench/workbench.h>
#include <nx_ec/abstract_ec_connection.h>
#include <nx_ec/data/api_conversion_functions.h>
#include <nx_ec/managers/abstract_layout_manager.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_layout_synchronizer.h>

namespace nx::vms::client::desktop {

LayoutSnapshotManager::LayoutSnapshotManager(SystemContext* systemContext, QObject* parent):
    base_type(parent),
    SystemContextAware(systemContext)
{
    // Start listening to changes.
    connect(resourcePool(), &QnResourcePool::resourcesAdded, this,
        &LayoutSnapshotManager::onResourcesAdded);
    connect(resourcePool(), &QnResourcePool::resourcesRemoved, this,
        &LayoutSnapshotManager::onResourcesRemoved);

    onResourcesAdded(resourcePool()->getResources<LayoutResource>());

    connect(this, &core::SaveStateManager::flagsChanged, this,
        [this](const QnUuid& id, SaveStateFlags /*flags*/)
        {
            if (const auto layout = resourcePool()->getResourceById<LayoutResource>(id))
                emit layoutFlagsChanged(layout);
        });
}

LayoutSnapshotManager::~LayoutSnapshotManager()
{
}

core::SaveStateManager::SaveStateFlags LayoutSnapshotManager::flags(
    const LayoutResourcePtr& layout) const
{
    NX_ASSERT(layout);
    if (!layout)
        return SaveStateFlags();
    return base_type::flags(layout->getId());
}

core::SaveStateManager::SaveStateFlags LayoutSnapshotManager::flags(
    QnWorkbenchLayout* layout) const
{
    NX_ASSERT(layout); //< Layout resource can be null.
    if (!layout || !layout->resource())
        return SaveStateFlags();

    return flags(layout->resource());
}

void LayoutSnapshotManager::setFlags(const LayoutResourcePtr& layout,
    SaveStateFlags flags)
{
    NX_ASSERT(layout && layout->resourcePool(),
        "Could not set flags to resource which is not in pool");

    if (!layout)
        return;

    base_type::setFlags(layout->getId(), flags);
}

bool LayoutSnapshotManager::isChanged(const LayoutResourcePtr &layout) const
{
    return base_type::isChanged(layout->getId());
}

bool LayoutSnapshotManager::isSaveable(const LayoutResourcePtr &layout) const
{
    if (layout->hasFlags(Qn::local))
        return true;

    return base_type::isSaveable(layout->getId());
}

bool LayoutSnapshotManager::isModified(const LayoutResourcePtr &layout) const
{
    /* Changed and not being saved. */
    return base_type::isSaveable(layout->getId());
}

bool LayoutSnapshotManager::save(
    const LayoutResourcePtr& layout,
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

    // Submit all changes from workbench to resource.
    // TODO: #sivanov system-context class should not depend on workbench.
    if (auto synchronizer = QnWorkbenchLayoutSynchronizer::instance(layout))
        synchronizer->submit();

    if (auto crossSystemLayout = layout.dynamicCast<CrossSystemLayoutResource>())
    {
        appContext()->cloudLayoutsManager()->saveLayout(crossSystemLayout, internalCallback);
        markBeingSaved(layout->getId(), true);
        return true;
    }

    auto connection = this->connection()->messageBusConnection();
    if (!NX_ASSERT(connection))
        return false;

    nx::vms::api::LayoutData apiLayout;
    ec2::fromResourceToApi(layout, apiLayout);

    int reqID = connection->getLayoutManager(Qn::kSystemAccess)->save(
        apiLayout,
        [internalCallback](int /*reqID*/, ec2::ErrorCode errorCode)
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

void LayoutSnapshotManager::store(const LayoutResourcePtr &resource)
{
    if (!NX_ASSERT(resource))
        return;

    resource->storeSnapshot();
    markChanged(resource->getId(), false);
}

void LayoutSnapshotManager::update(
    const LayoutResourcePtr& resource, const QnLayoutResourcePtr& remoteResource)
{
    if (!NX_ASSERT(resource && remoteResource))
        return;

    resource->updateSnapshot(remoteResource);

    // Remote layout was changed and may differ from the local one now.
    markChanged(resource->getId(), true);
}

void LayoutSnapshotManager::restore(const LayoutResourcePtr &resource)
{
    if (!NX_ASSERT(resource))
        return;

    /* We don't want to get queued layout change signals for these changes,
     * so there are no options but to disconnect before making them. */
    disconnectFrom(resource);
    {
        const auto snapshot = resource->snapshot();
        QnLayoutResourcePtr restored(new LayoutResource());
        ec2::fromApiToResource(snapshot, restored);

        // Overwrite flags as they are runtime data and not stored in the snapshot.
        restored->setFlags(resource->flags());

        // Cleanup from snapshot resources which are already deleted from the resource pool.
        common::LayoutItemDataMap existingItems;
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

void LayoutSnapshotManager::connectTo(const LayoutResourcePtr &resource)
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
}

void LayoutSnapshotManager::disconnectFrom(const LayoutResourcePtr &resource)
{
    resource->disconnect(this);
}

// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void LayoutSnapshotManager::onResourcesAdded(const QnResourceList& resources)
{
    for (const auto& layout: resources.filtered<LayoutResource>())
    {
        clean(layout->getId()); //< Not changed, not being saved.

        /* Subscribe to changes to track changed status. */
        connectTo(layout);
    }
}

void LayoutSnapshotManager::onResourcesRemoved(const QnResourceList& resources)
{
    for (const auto& layout: resources.filtered<LayoutResource>())
    {
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
    auto layoutResource = resource.dynamicCast<QnLayoutResource>();
    if (NX_ASSERT(layoutResource))
        at_layout_changed(layoutResource);
}

} // namespace nx::vms::client::desktop
