// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "videowall_resource.h"

#include <nx/vms/api/data/videowall_data.h>

QnVideoWallResource::QnVideoWallResource():
    base_type(),
    m_items(new QnThreadsafeItemStorage<QnVideoWallItem>(&m_mutex, this)),
    m_pcs(new QnThreadsafeItemStorage<QnVideoWallPcData>(&m_mutex, this)),
    m_matrices(new QnThreadsafeItemStorage<QnVideoWallMatrix>(&m_mutex, this))
{
    addFlags(Qn::videowall | Qn::remote);
    setTypeId(nx::vms::api::VideowallData::kResourceTypeId);
}

nx::vms::api::ResourceStatus QnVideoWallResource::getStatus() const
{
    return nx::vms::api::ResourceStatus::online;
}

QnThreadsafeItemStorage<QnVideoWallItem> * QnVideoWallResource::items() const
{
    return m_items.data();
}

QnThreadsafeItemStorage<QnVideoWallPcData> * QnVideoWallResource::pcs() const
{
    return m_pcs.data();
}

QnThreadsafeItemStorage<QnVideoWallMatrix> * QnVideoWallResource::matrices() const
{
    return m_matrices.data();
}

void QnVideoWallResource::updateInternal(const QnResourcePtr& source, NotifierList& notifiers)
{
    base_type::updateInternal(source, notifiers);

    QnVideoWallResource* localOther = dynamic_cast<QnVideoWallResource*>(source.data());
    if (localOther)
    {
        // Copy online status to the updated items.
        auto newItems = localOther->items();
        const auto existingItems = m_items->getItems();
        for (const auto& item: existingItems)
        {
            if (newItems->hasItem(item.uuid))
            {
                QnVideoWallItem newItem = newItems->getItem(item.uuid);
                newItem.runtimeStatus.online = item.runtimeStatus.online;
                newItem.runtimeStatus.controlledBy = item.runtimeStatus.controlledBy;
                newItems->updateItem(newItem);
            }
        }
        QnThreadsafeItemStorageNotifier<QnVideoWallItem>::
            setItemsUnderLockInternal(m_items.data(), newItems, notifiers);
        QnThreadsafeItemStorageNotifier<QnVideoWallPcData>::
            setItemsUnderLockInternal(m_pcs.data(), localOther->pcs(), notifiers);
        QnThreadsafeItemStorageNotifier<QnVideoWallMatrix>::
            setItemsUnderLockInternal(m_matrices.data(), localOther->matrices(), notifiers);

        if (m_autorun != localOther->m_autorun)
        {
            m_autorun = localOther->m_autorun;
            notifiers << [r = toSharedPointer(this)]{ emit r->autorunChanged(r); };
        }

        if (m_timelineEnabled != localOther->m_timelineEnabled)
        {
            m_timelineEnabled = localOther->m_timelineEnabled;
            notifiers << [r = toSharedPointer(this)]{ emit r->timelineEnabledChanged(r); };
        }
    }
}

bool QnVideoWallResource::isAutorun() const
{
    NX_MUTEX_LOCKER locker(&m_mutex);
    return m_autorun;
}

void QnVideoWallResource::setAutorun(bool value)
{
    {
        NX_MUTEX_LOCKER locker(&m_mutex);
        if (m_autorun == value)
            return;
        m_autorun = value;
    }
    emit autorunChanged(::toSharedPointer(this));
}

bool QnVideoWallResource::isTimelineEnabled() const
{
    NX_MUTEX_LOCKER locker(&m_mutex);
    return m_timelineEnabled;
}

void QnVideoWallResource::setTimelineEnabled(bool value)
{
    {
        NX_MUTEX_LOCKER locker(&m_mutex);
        if (m_timelineEnabled == value)
            return;
        m_timelineEnabled = value;
    }
    emit timelineEnabledChanged(::toSharedPointer(this));
}

Qn::Notifier QnVideoWallResource::storedItemAdded(const QnVideoWallItem& item)
{
    return [r = toSharedPointer(this), item]{ emit r->itemAdded(r, item); };
}

Qn::Notifier QnVideoWallResource::storedItemRemoved(const QnVideoWallItem& item)
{
    return [r = toSharedPointer(this), item]
        {
            // removing item from the matrices and removing empty matrices (if any)
            const QnVideoWallMatrixMap matrices = r->matrices()->getItems();
            for (const QnVideoWallMatrix& matrix: matrices)
            {
                if (!matrix.layoutByItem.contains(item.uuid))
                    continue;

                QnVideoWallMatrix updatedMatrix(matrix);
                updatedMatrix.layoutByItem.remove(item.uuid);

                if (updatedMatrix.layoutByItem.isEmpty())
                    r->matrices()->removeItem(matrix.uuid);
                else
                    r->matrices()->updateItem(updatedMatrix);
            }

            emit r->itemRemoved(r, item);

            const auto& items = r->items()->getItems();
            const bool lastItemFromThisPc = std::all_of(
                items.begin(), items.end(),
                [pcUuid = item.pcUuid](const QnVideoWallItem& item)
                {
                    return item.pcUuid != pcUuid;
                });

            if (lastItemFromThisPc)
                r->pcs()->removeItem(item.pcUuid);
        };
}

Qn::Notifier QnVideoWallResource::storedItemChanged(const QnVideoWallItem& item,
    const QnVideoWallItem& oldItem)
{
    return [r = toSharedPointer(this), item, oldItem]{ emit r->itemChanged(r, item, oldItem); };
}

Qn::Notifier QnVideoWallResource::storedItemAdded(const QnVideoWallPcData& item)
{
    return [r = toSharedPointer(this), item]{ emit r->pcAdded(r, item); };
}

Qn::Notifier QnVideoWallResource::storedItemRemoved(const QnVideoWallPcData& item)
{
    return [r = toSharedPointer(this), item]{ emit r->pcRemoved(r, item); };
}

Qn::Notifier QnVideoWallResource::storedItemChanged(
    const QnVideoWallPcData& item,
    const QnVideoWallPcData& /*oldItem*/)
{
    return [r = toSharedPointer(this), item]{ emit r->pcChanged(r, item); };
}

Qn::Notifier QnVideoWallResource::storedItemAdded(const QnVideoWallMatrix& item)
{
    return [r = toSharedPointer(this), item]{ emit r->matrixAdded(r, item); };
}

Qn::Notifier QnVideoWallResource::storedItemRemoved(const QnVideoWallMatrix& item)
{
    return [r = toSharedPointer(this), item]{ emit r->matrixRemoved(r, item); };
}

Qn::Notifier QnVideoWallResource::storedItemChanged(
    const QnVideoWallMatrix& item,
    const QnVideoWallMatrix& oldItem)
{
    return [r = toSharedPointer(this), item, oldItem]{ emit r->matrixChanged(r, item, oldItem); };
}

QList<QnUuid> QnVideoWallResource::onlineItems() const
{
    QList<QnUuid> result;
    const auto items = m_items->getItems();
    for (const QnVideoWallItem& item: items)
    {
        if (!item.runtimeStatus.online)
            continue;
        result.append(item.uuid);
    }
    return result;
}
