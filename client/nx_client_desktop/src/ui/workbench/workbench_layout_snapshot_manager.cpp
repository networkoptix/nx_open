#include "workbench_layout_snapshot_manager.h"

#include <cassert>

#include <api/app_server_connection.h>

#include <common/common_module.h>

#include <core/resource/layout_resource.h>
#include <core/resource_management/resource_pool.h>

#include <nx_ec/data/api_layout_data.h>
#include <nx_ec/data/api_conversion_functions.h>
#include <nx_ec/managers/abstract_layout_manager.h>

#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_layout_snapshot_storage.h>
#include <ui/workbench/workbench_layout_synchronizer.h>
#include <ui/workbench/workbench_layout.h>

#include <utils/common/warnings.h>
#include <utils/common/checked_cast.h>

// -------------------------------------------------------------------------- //
// QnWorkbenchLayoutSnapshotManager
// -------------------------------------------------------------------------- //
QnWorkbenchLayoutSnapshotManager::QnWorkbenchLayoutSnapshotManager(QObject *parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    m_storage(new QnWorkbenchLayoutSnapshotStorage(this))
{
    /* Start listening to changes. */
    connect(resourcePool(), &QnResourcePool::resourceRemoved, this,
        &QnWorkbenchLayoutSnapshotManager::at_resourcePool_resourceRemoved);
    connect(resourcePool(), &QnResourcePool::resourceAdded, this,
        &QnWorkbenchLayoutSnapshotManager::at_resourcePool_resourceAdded);

    for (const QnLayoutResourcePtr &layout : resourcePool()->getResources<QnLayoutResource>())
        at_resourcePool_resourceAdded(layout);

    connect(this, &QnAbstractSaveStateManager::flagsChanged, this,
        [this](const QnUuid& id, SaveStateFlags /*flags*/)
        {
            if (const auto layout = resourcePool()->getResourceById<QnLayoutResource>(id))
                emit layoutFlagsChanged(layout);
        });
}

QnWorkbenchLayoutSnapshotManager::~QnWorkbenchLayoutSnapshotManager()
{
}

QnAbstractSaveStateManager::SaveStateFlags QnWorkbenchLayoutSnapshotManager::flags(
    const QnLayoutResourcePtr& layout) const
{
    NX_EXPECT(layout);
    if (!layout)
        return SaveStateFlags();
    return base_type::flags(layout->getId());
}

QnAbstractSaveStateManager::SaveStateFlags QnWorkbenchLayoutSnapshotManager::flags(
    QnWorkbenchLayout* layout) const
{
    NX_EXPECT(layout); //< Layout resource can be null.
    if (!layout || !layout->resource())
        return SaveStateFlags();

    return flags(layout->resource());
}

void QnWorkbenchLayoutSnapshotManager::setFlags(const QnLayoutResourcePtr& layout,
    SaveStateFlags flags)
{
    NX_EXPECT(layout && layout->resourcePool(),
        "Could not set flags to resource which is not in pool");

    if (!layout)
        return;

    NX_EXPECT(!flags.testFlag(IsBeingSaved)
        || accessController()->hasPermissions(layout, Qn::SavePermission),
        "Saving unsaveable resource");

    base_type::setFlags(layout->getId(), flags);
}

bool QnWorkbenchLayoutSnapshotManager::isChanged(const QnLayoutResourcePtr &layout) const
{
    return base_type::isChanged(layout->getId());
}

bool QnWorkbenchLayoutSnapshotManager::isSaveable(const QnLayoutResourcePtr &layout) const
{
    if (layout->hasFlags(Qn::local))
        return true;

    return base_type::isSaveable(layout->getId());
}

bool QnWorkbenchLayoutSnapshotManager::isModified(const QnLayoutResourcePtr &layout) const
{
    /* Changed and not being saved. */
    return base_type::isSaveable(layout->getId());
}

bool QnWorkbenchLayoutSnapshotManager::save(const QnLayoutResourcePtr &layout, SaveLayoutResultFunction callback)
{
    NX_EXPECT(accessController()->hasPermissions(layout, Qn::SavePermission),
        "Saving unsaveable resource");

    if (commonModule()->isReadOnly())
        return false;

    NX_ASSERT(!layout->isFile());
    if (layout->isFile())
        return false;

    /* Submit all changes from workbench to resource. */
    if (QnWorkbenchLayoutSynchronizer *synchronizer = QnWorkbenchLayoutSynchronizer::instance(layout))
        synchronizer->submit();

    ec2::ApiLayoutData apiLayout;
    ec2::fromResourceToApi(layout, apiLayout);

    int reqID = commonModule()->ec2Connection()->getLayoutManager(Qn::kSystemAccess)->save(
        apiLayout, this, [this, layout, callback](int /*reqID*/, ec2::ErrorCode errorCode)
        {
            markBeingSaved(layout->getId(), false);

            /* Check if all OK */
            bool success = errorCode == ec2::ErrorCode::ok;
            if (success)
            {
                store(layout); //< Cleanup 'changed' flag here, sending corresponding signal.
                clean(layout->getId()); //< Silently remove from flags storage.
            }

            if (callback)
                callback(success, layout);
        });
    bool success = reqID > 0;
    if (success)
        markBeingSaved(layout->getId(), true);
    return success;
}

QnWorkbenchLayoutSnapshot QnWorkbenchLayoutSnapshotManager::snapshot(const QnLayoutResourcePtr &layout) const
{
    NX_ASSERT(layout);
    if (!layout)
        return QnWorkbenchLayoutSnapshot();
    return m_storage->snapshot(layout);
}

void QnWorkbenchLayoutSnapshotManager::store(const QnLayoutResourcePtr &resource)
{
    if (!resource)
    {
        qnNullWarning(resource);
        return;
    }

    /* We don't want to get queued layout change signals that are not yet delivered,
     * so there are no options but to disconnect. */
    disconnectFrom(resource);
    m_storage->store(resource);
    connectTo(resource);
    markChanged(resource->getId(), false);
}

void QnWorkbenchLayoutSnapshotManager::restore(const QnLayoutResourcePtr &resource)
{
    if (!resource)
    {
        qnNullWarning(resource);
        return;
    }

    /* We don't want to get queued layout change signals for these changes,
     * so there are no options but to disconnect before making them. */
    disconnectFrom(resource);
    {
        const QnWorkbenchLayoutSnapshot &snapshot = m_storage->snapshot(resource);

        // Cleanup from snapshot resources which are already deleted from the resource pool.
        QnLayoutItemDataMap existingItems;
        for (const auto item: snapshot.items)
        {
            if (!resourcePool()->getResourceByDescriptor(item.resource))
                continue;
            existingItems.insert(item.uuid, item);
        }

        resource->setItems(existingItems);
        resource->setName(snapshot.name);
        resource->setCellAspectRatio(snapshot.cellAspectRatio);
        resource->setCellSpacing(snapshot.cellSpacing);
        resource->setBackgroundSize(snapshot.backgroundSize);
        resource->setBackgroundImageFilename(snapshot.backgroundImageFilename);
        resource->setBackgroundOpacity(snapshot.backgroundOpacity);
        resource->setLocked(snapshot.locked);
    }
    connectTo(resource);

    if (QnWorkbenchLayoutSynchronizer *synchronizer = QnWorkbenchLayoutSynchronizer::instance(resource))
        synchronizer->reset();

    markChanged(resource->getId(), false);
}

void QnWorkbenchLayoutSnapshotManager::connectTo(const QnLayoutResourcePtr &resource)
{
    connect(resource, &QnResource::nameChanged, this, &QnWorkbenchLayoutSnapshotManager::at_resource_changed);
    connect(resource, &QnResource::parentIdChanged, this, &QnWorkbenchLayoutSnapshotManager::at_resource_changed);
    connect(resource, &QnLayoutResource::itemAdded, this, &QnWorkbenchLayoutSnapshotManager::at_layout_changed);
    connect(resource, &QnLayoutResource::itemRemoved, this, &QnWorkbenchLayoutSnapshotManager::at_layout_changed);
    connect(resource, &QnLayoutResource::lockedChanged, this, &QnWorkbenchLayoutSnapshotManager::at_layout_changed);
    connect(resource, &QnLayoutResource::cellAspectRatioChanged, this, &QnWorkbenchLayoutSnapshotManager::at_layout_changed);
    connect(resource, &QnLayoutResource::cellSpacingChanged, this, &QnWorkbenchLayoutSnapshotManager::at_layout_changed);
    connect(resource, &QnLayoutResource::backgroundImageChanged, this, &QnWorkbenchLayoutSnapshotManager::at_layout_changed);
    connect(resource, &QnLayoutResource::backgroundSizeChanged, this, &QnWorkbenchLayoutSnapshotManager::at_layout_changed);
    connect(resource, &QnLayoutResource::backgroundOpacityChanged, this, &QnWorkbenchLayoutSnapshotManager::at_layout_changed);

    /* This one is handled separately because it should not lead to marking layout as modified if layout is not opened right now. */
    connect(resource, &QnLayoutResource::itemChanged, this, &QnWorkbenchLayoutSnapshotManager::at_layout_itemChanged);

    connect(resource, &QnLayoutResource::storeRequested, this, &QnWorkbenchLayoutSnapshotManager::store);
}

void QnWorkbenchLayoutSnapshotManager::disconnectFrom(const QnLayoutResourcePtr &resource)
{
    resource->disconnect(this);
}

// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnWorkbenchLayoutSnapshotManager::at_resourcePool_resourceAdded(const QnResourcePtr &resource)
{
    QnLayoutResourcePtr layoutResource = resource.dynamicCast<QnLayoutResource>();
    if (!layoutResource)
        return;

    /* Consider it saved by default. */
    m_storage->store(layoutResource);

    clean(layoutResource->getId()); //< Not changed, not being saved.

    /* Subscribe to changes to track changed status. */
    connectTo(layoutResource);
}

void QnWorkbenchLayoutSnapshotManager::at_resourcePool_resourceRemoved(const QnResourcePtr &resource)
{
    if (const auto layout = resource.dynamicCast<QnLayoutResource>())
    {
        m_storage->remove(layout);
        clean(layout->getId());
        disconnectFrom(layout);
    }
}

void QnWorkbenchLayoutSnapshotManager::at_layout_changed(const QnLayoutResourcePtr& layout)
{
    markChanged(layout->getId(), true);
}

void QnWorkbenchLayoutSnapshotManager::at_resource_changed(const QnResourcePtr &resource)
{
    QnLayoutResourcePtr layoutResource = resource.dynamicCast<QnLayoutResource>();
    NX_ASSERT(layoutResource);
    if (layoutResource)
        at_layout_changed(layoutResource);
}

void QnWorkbenchLayoutSnapshotManager::at_layout_itemChanged(const QnLayoutResourcePtr &resource)
{
    if (workbench()->currentLayout()->resource() == resource)
        at_layout_changed(resource);
}
