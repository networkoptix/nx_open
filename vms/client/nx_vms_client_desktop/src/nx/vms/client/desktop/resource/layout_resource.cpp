// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "layout_resource.h"

#include <QtCore/QCoreApplication>

#include <core/resource/client_camera.h>
#include <core/resource/user_resource.h>
#include <core/resource/videowall_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/network/rest/user_access_data.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/qt_helpers.h>
#include <nx/vms/api/data/layout_data.h>
#include <nx/vms/client/core/network/remote_connection.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/resource/resource_descriptor.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/common/intercom/utils.h>
#include <nx_ec/abstract_ec_connection.h>
#include <nx_ec/data/api_conversion_functions.h>
#include <nx_ec/managers/abstract_layout_manager.h>

namespace nx::vms::client::desktop {

namespace {

class SnapshotLayoutResource: public QnLayoutResource
{
    const LayoutResourcePtr m_parentLayout;

public:
    SnapshotLayoutResource(const LayoutResourcePtr& parentLayout): m_parentLayout(parentLayout) {}
    virtual QnLayoutResourcePtr transientLayout() const override { return m_parentLayout; }
};

const auto unsavedChangedGuard(const LayoutResourcePtr& layout)
{
    return nx::utils::ScopeGuard(
        [layout, wasUnsaved = layout ? layout->canBeSaved() : false]
        {
            if (layout && layout->canBeSaved() != wasUnsaved)
                emit layout->canBeSavedChanged(layout);
        });
}

} // namespace

qreal LayoutResource::cellSpacingValue(Qn::CellSpacing spacing)
{
    switch (spacing)
    {
        case Qn::CellSpacing::None:
            return 0.0;
        case Qn::CellSpacing::Small:
            return nx::vms::api::LayoutData::kDefaultCellSpacing;
        case Qn::CellSpacing::Medium:
            return nx::vms::api::LayoutData::kDefaultCellSpacing * 2;
        case Qn::CellSpacing::Large:
            return nx::vms::api::LayoutData::kDefaultCellSpacing * 3;
    }
    NX_ASSERT(false, "Unhandled enum value");
    return nx::vms::api::LayoutData::kDefaultCellSpacing;
}

LayoutResource::LayoutResource()
{
    connect(this, &QnLayoutResource::parentIdChanged, this, &LayoutResource::updateLayoutType);

    connect(this, &QnResource::nameChanged, this, &LayoutResource::handleLayoutChange);
    connect(this, &QnResource::parentIdChanged, this, &LayoutResource::handleLayoutChange);
    connect(this, &QnResource::logicalIdChanged, this, &LayoutResource::handleLayoutChange);
    connect(this, &QnLayoutResource::lockedChanged, this, &LayoutResource::handleLayoutChange);
    connect(this, &QnLayoutResource::fixedSizeChanged, this, &LayoutResource::handleLayoutChange);

    connect(this, &QnLayoutResource::cellAspectRatioChanged,
        this, &LayoutResource::handleLayoutChange);

    connect(this, &QnLayoutResource::cellSpacingChanged,
        this, &LayoutResource::handleLayoutChange);

    connect(this, &QnLayoutResource::backgroundImageChanged,
        this, &LayoutResource::handleLayoutChange);

    connect(this, &QnLayoutResource::backgroundSizeChanged,
        this, &LayoutResource::handleLayoutChange);

    connect(this, &QnLayoutResource::backgroundOpacityChanged,
        this, &LayoutResource::handleLayoutChange);

    connect(this, &QnLayoutResource::itemAdded, this, &LayoutResource::handleLayoutChange);
    connect(this, &QnLayoutResource::itemRemoved, this, &LayoutResource::handleLayoutChange);
    connect(this, &QnLayoutResource::itemChanged, this, &LayoutResource::handleLayoutChange);
}

void LayoutResource::setPredefinedCellSpacing(Qn::CellSpacing spacing)
{
    setCellSpacing(cellSpacingValue(spacing));
}

common::LayoutItemDataList LayoutResource::cloneItems(
    common::LayoutItemDataList items,
    ItemsRemapHash* remapHash)
{
    ItemsRemapHash newUuidByOldUuid;
    for (int i = 0; i < items.size(); i++)
    {
        nx::Uuid newUuid = nx::Uuid::createUuid();
        newUuidByOldUuid[items[i].uuid] = newUuid;
        items[i].uuid = newUuid;
    }
    for (int i = 0; i < items.size(); i++)
        items[i].zoomTargetUuid = newUuidByOldUuid.value(items[i].zoomTargetUuid, nx::Uuid());
    if (remapHash)
        *remapHash = newUuidByOldUuid;
    return items;
}

void LayoutResource::cloneItems(LayoutResourcePtr target, ItemsRemapHash* remapHash) const
{
    target->setItems(cloneItems(m_items->getItems().values(), remapHash));
}

void LayoutResource::cloneTo(LayoutResourcePtr target, ItemsRemapHash* remapHash) const
{
    {
        NX_MUTEX_LOCKER locker(&m_mutex);
        target->setIdUnsafe(nx::Uuid::createUuid());
        target->setUrl(m_url);
        target->setName(m_name);
        target->setParentId(m_parentId);
        target->setCellSpacing(m_cellSpacing);
        target->setCellAspectRatio(m_cellAspectRatio);
        target->setBackgroundImageFilename(m_backgroundImageFilename);
        target->setBackgroundOpacity(m_backgroundOpacity);
        target->setBackgroundSize(m_backgroundSize);
    }

    cloneItems(target, remapHash);
}

LayoutResourcePtr LayoutResource::clone(ItemsRemapHash* remapHash) const
{
    LayoutResourcePtr result = create();
    cloneTo(result, remapHash);
    return result;
}

nx::vms::api::LayoutData LayoutResource::snapshot() const
{
    NX_MUTEX_LOCKER locker(&m_mutex);
    return m_snapshot;
}

void LayoutResource::storeSnapshot()
{
    updateSnapshot(toSharedPointer(this));
}

void LayoutResource::updateSnapshot(const QnLayoutResourcePtr& remoteLayout)
{
    if (!NX_ASSERT(remoteLayout))
        return;

    auto snapshotLayout = storedLayout(); //< Ensure the stored layout object is created.
    const auto guard = unsavedChangedGuard(toSharedPointer(this));
    {
        NX_MUTEX_LOCKER locker(&m_mutex);
        ec2::fromResourceToApi(remoteLayout, m_snapshot);
        ec2::fromApiToResource(snapshot(), snapshotLayout);
        m_changed = remoteLayout != this;
    }
}

void LayoutResource::restoreFromSnapshot()
{
    const auto guard = unsavedChangedGuard(toSharedPointer(this));
    NotifierList notifiers;

    {
        NX_MUTEX_LOCKER locker(&m_mutex);

        QnLayoutResourcePtr restored(new LayoutResource());
        ec2::fromApiToResource(snapshot(), restored);

        // Overwrite flags as they are runtime data and not stored in the snapshot.
        restored->setFlags(flags());

        // Cleanup from snapshot resources which are already deleted from the resource pool.
        common::LayoutItemDataMap existingItems;
        for (const auto& item: restored->getItems())
        {
            if (getResourceByDescriptor(item.resource))
                existingItems.insert(item.uuid, item);
        }

        restored->setItems(existingItems);
        updateInternal(restored, notifiers); //< No need to lock the mutex of `restored`.

        m_changed = false;
    }

    for (const auto& notifier: notifiers)
        notifier();

    emit layoutRestored(toSharedPointer(this), QPrivateSignal{});
}

bool LayoutResource::isChanged() const
{
    return m_changed;
}

bool LayoutResource::canBeSaved() const
{
    return ((hasFlags(Qn::local) && !hasFlags(Qn::exported_layout)) || m_changed) && !m_beingSaved;
}

// This function itself is thread-safe, but QnWorkbenchLayout's handler of `layoutAboutToBeSaved`
// signal is currently not thread-safe, it expects the signal sent from the main thread only.
bool LayoutResource::saveAsync(SaveLayoutResultFunction callback)
{
    const auto thisPtr = toSharedPointer(this);
    const auto guard = unsavedChangedGuard(thisPtr);

    if (!NX_ASSERT(!isFile())
        || !NX_ASSERT(QThread::currentThread() == qApp->thread())
        || m_beingSaved.exchange(true)) //< If already being saved.
    {
        return false;
    }

    const auto internalCallback = nx::utils::guarded(this,
        [callback](bool success, const LayoutResourcePtr& layout)
        {
            const auto guard = unsavedChangedGuard(layout);
            if (success)
                layout->storeSnapshot();

            if (callback)
                callback(success, layout);

            NX_ASSERT(layout->m_beingSaved.exchange(false));
            emit layout->layoutSaved(success, layout, QPrivateSignal{});
        });

    emit layoutAboutToBeSaved(thisPtr, QPrivateSignal{});

    if (!doSaveAsync(internalCallback))
    {
        NX_ASSERT(m_beingSaved.exchange(false));
        emit layoutSaved(/*success*/ false, thisPtr, QPrivateSignal{});
        return false;
    }

    return true;
}

bool LayoutResource::doSaveAsync(SaveLayoutResultFunction callback)
{
    const auto systemContext = qobject_cast<SystemContext*>(this->systemContext());
    const auto connection = NX_ASSERT(systemContext)
        ? systemContext->connection()->messageBusConnection()
        : ec2::AbstractECConnectionPtr{};

    if (!NX_ASSERT(connection))
        return false;

    const auto thisPtr = toSharedPointer(this);

    nx::vms::api::LayoutData data;
    ec2::fromResourceToApi(thisPtr, data);

    const int request = connection->getLayoutManager(nx::network::rest::kSystemSession)->save(data,
        [layout = thisPtr, callback](int /*reqID*/, ec2::ErrorCode errorCode)
        {
            const bool success = errorCode == ec2::ErrorCode::ok;
            callback(success, layout);
        },
        this);

    const bool result = request > 0;
    return result;
}

QnLayoutResourcePtr LayoutResource::storedLayout() const
{
    NX_MUTEX_LOCKER locker(&m_mutex);
    if (!m_snapshotLayout)
        m_snapshotLayout.reset(new SnapshotLayoutResource(toSharedPointer(this)));

    return m_snapshotLayout;
}

QnTimePeriod LayoutResource::localRange() const
{
    NX_MUTEX_LOCKER locker(&m_mutex);
    return m_localRange;
}

void LayoutResource::setLocalRange(const QnTimePeriod& value)
{
    NX_MUTEX_LOCKER locker(&m_mutex);
    m_localRange = value;
}

LayoutResource::DataHash LayoutResource::data() const
{
    NX_MUTEX_LOCKER locker(&m_mutex);
    return m_data;
}

void LayoutResource::setData(const DataHash& data)
{
    QSet<int> updatedRoles;

    {
        NX_MUTEX_LOCKER locker(&m_mutex);

        updatedRoles = nx::utils::toQSet(data.keys() + m_data.keys());
        m_data = data;
    }

    for (const auto role: updatedRoles)
        emit dataChanged(role);
}

QVariant LayoutResource::data(int role) const
{
    NX_MUTEX_LOCKER locker(&m_mutex);

    const auto it = m_data.find(role);
    return it != m_data.end()
        ? it.value()
        : QVariant();
}

void LayoutResource::setData(int role, const QVariant &value)
{
    {
        NX_MUTEX_LOCKER locker(&m_mutex);
        m_data[role] = value;
    }
    emit dataChanged(role);
}

QVariant LayoutResource::itemData(const nx::Uuid& id, int role) const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_itemData.value(id).value(role);
}

void LayoutResource::setItemData(const nx::Uuid& id, int role, const QVariant& data)
{
    bool notify = false;
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        notify = setItemDataUnderLock(id, role, data);
    }

    if (notify)
         emit itemDataChanged(id, role, data);
}

void LayoutResource::cleanupItemData(const nx::Uuid& id)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    m_itemData.remove(id);
}

void LayoutResource::cleanupItemData()
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    m_itemData.clear();
}

bool LayoutResource::isPreviewSearchLayout() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_data.contains(Qn::LayoutSearchStateRole);
}

bool LayoutResource::isShowreelReviewLayout() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_data.contains(Qn::ShowreelUuidRole);
}

bool LayoutResource::isVideoWallReviewLayout() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_data.contains(Qn::VideoWallResourceRole);
}

LayoutResource::LayoutType LayoutResource::layoutType() const
{
    return m_layoutType;
}

bool LayoutResource::isCrossSystem() const
{
    return hasFlags(Qn::cross_system);
}

bool LayoutResource::setItemDataUnderLock(
    const nx::Uuid& id,
    int role,
    const QVariant& data)
{
    const QVariant& oldData = m_itemData.value(id).value(role);
    if (data.isValid() && oldData == data)
        return false;

    if (data.isValid())
    {
        m_itemData[id][role] = data;
    }
    else
    {
        if (!m_itemData.contains(id))
            return false;

        if (!m_itemData[id].contains(role))
            return false;

        m_itemData[id].remove(role);
        if (m_itemData[id].isEmpty())
            m_itemData.remove(id);
    }

    return true;
}

LayoutResourcePtr LayoutResource::create() const
{
    return LayoutResourcePtr(new LayoutResource());
}

void LayoutResource::setSystemContext(nx::vms::common::SystemContext* systemContext)
{
    QnLayoutResource::setSystemContext(systemContext);
    storeSnapshot();
    updateLayoutType();
}

void LayoutResource::updateInternal(const QnResourcePtr& source, NotifierList& notifiers)
{
    QnLayoutResource::updateInternal(source, notifiers);

    if (!notifiers.empty())
    {
        const bool wasUnsaved = canBeSaved();
        m_changed = true;
        if (wasUnsaved != canBeSaved())
            notifiers << [r = toSharedPointer(this)]{ emit r->canBeSavedChanged(r); };
    }

    const auto localOther = source.dynamicCast<LayoutResource>();
    if (NX_ASSERT(localOther))
        m_localRange = localOther->m_localRange;
}

void LayoutResource::handleLayoutChange()
{
    const auto guard = unsavedChangedGuard(toSharedPointer(this));
    m_changed = true;
}

void LayoutResource::setLayoutType(LayoutType value)
{
    if (m_layoutType == value)
        return;

    m_layoutType = value;
    emit layoutTypeChanged(toSharedPointer(this));
}

LayoutResource::LayoutType LayoutResource::calculateLayoutType() const
{
    if (isFile())
        return LayoutType::file;

    if (isShared())
        return LayoutType::shared;

    const auto parentResource = getParentResource();
    if (!parentResource)
        return LayoutType::unknown;

    if (const auto parentUser = parentResource.objectCast<QnUserResource>())
        return LayoutType::local;

    if (const auto parentVideoWall = parentResource.objectCast<QnVideoWallResource>())
        return LayoutType::videoWall;

    if (const auto parentCamera = parentResource.objectCast<QnClientCameraResource>())
    {
        return NX_ASSERT(parentCamera->isIntercom())
            ? LayoutType::intercom
            : LayoutType::invalid;
    }

    NX_ASSERT(false, "Unexpected layout parent: %1", parentResource);
    return LayoutType::invalid;
}

void LayoutResource::updateLayoutType()
{
    setLayoutType(calculateLayoutType());

    NX_MUTEX_LOCKER lk(&m_mutex);
    m_resourcePoolConnection.reset();

    if (m_layoutType == LayoutType::unknown && resourcePool())
    {
        m_resourcePoolConnection.reset(
            connect(resourcePool(), &QnResourcePool::resourcesAdded, this,
                [this]()
                {
                    if (getParentResource())
                        updateLayoutType();
                },
                Qt::DirectConnection));
    }
}

} // namespace nx::vms::client::desktop
