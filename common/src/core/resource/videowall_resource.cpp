#include "videowall_resource.h"

QnVideoWallResource::QnVideoWallResource(QnCommonModule* commonModule):
    base_type(commonModule),
    m_autorun(false),
    m_items(new QnThreadsafeItemStorage<QnVideoWallItem>(&m_mutex, this)),
    m_pcs(new QnThreadsafeItemStorage<QnVideoWallPcData>(&m_mutex, this)),
    m_matrices(new QnThreadsafeItemStorage<QnVideoWallMatrix>(&m_mutex, this))
{
    addFlags(Qn::videowall | Qn::remote);
    setTypeId(qnResTypePool->getFixedResourceTypeId(QnResourceTypePool::kVideoWallTypeId));
}

Qn::ResourceStatus QnVideoWallResource::getStatus() const
{
    return Qn::Online;
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

void QnVideoWallResource::updateInternal(const QnResourcePtr &other, Qn::NotifierList& notifiers)
{
    base_type::updateInternal(other, notifiers);

    QnVideoWallResource* localOther = dynamic_cast<QnVideoWallResource*>(other.data());
    if (localOther)
    {
        // copy online status to the updated items
        auto newItems = localOther->items();
        for (const auto &item : m_items->getItems())
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
    }
}

bool QnVideoWallResource::isAutorun() const
{
    QnMutexLocker locker(&m_mutex);
    return m_autorun;
}

void QnVideoWallResource::setAutorun(bool value)
{
    {
        QnMutexLocker locker(&m_mutex);
        if (m_autorun == value)
            return;
        m_autorun = value;
    }
    emit autorunChanged(::toSharedPointer(this));
}

Qn::Notifier QnVideoWallResource::storedItemAdded(const QnVideoWallItem &item)
{
    return [r = toSharedPointer(this), item]{ emit r->itemAdded(r, item); };
}

Qn::Notifier QnVideoWallResource::storedItemRemoved(const QnVideoWallItem &item)
{
    return [r = toSharedPointer(this), item]
        {
            // removing item from the matrices and removing empty matrices (if any)
            QnVideoWallMatrixMap matrices = r->matrices()->getItems();
            for (QnVideoWallMatrix matrix: matrices)
            {
                if (!matrix.layoutByItem.contains(item.uuid))
                    continue;
                matrix.layoutByItem.remove(item.uuid);
                if (matrix.layoutByItem.isEmpty())
                    r->matrices()->removeItem(matrix.uuid);
                else
                    r->matrices()->updateItem(matrix);
            }

            emit r->itemRemoved(r, item);

            const bool lastItemFromThisPc = boost::algorithm::all_of(r->items()->getItems(),
                [pcUuid = item.pcUuid](const QnVideoWallItem &item)
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

Qn::Notifier QnVideoWallResource::storedItemAdded(const QnVideoWallPcData &item)
{
    return [r = toSharedPointer(this), item]{ emit r->pcAdded(r, item); };
}

Qn::Notifier QnVideoWallResource::storedItemRemoved(const QnVideoWallPcData &item)
{
    return [r = toSharedPointer(this), item]{ emit r->pcRemoved(r, item); };
}

Qn::Notifier QnVideoWallResource::storedItemChanged(const QnVideoWallPcData& item)
{
    return [r = toSharedPointer(this), item]{ emit r->pcChanged(r, item); };
}

Qn::Notifier QnVideoWallResource::storedItemAdded(const QnVideoWallMatrix &item)
{
    return [r = toSharedPointer(this), item]{ emit r->matrixAdded(r, item); };
}

Qn::Notifier QnVideoWallResource::storedItemRemoved(const QnVideoWallMatrix &item)
{
    return [r = toSharedPointer(this), item]{ emit r->matrixRemoved(r, item); };
}

Qn::Notifier QnVideoWallResource::storedItemChanged(const QnVideoWallMatrix& item)
{
    return [r = toSharedPointer(this), item]{ emit r->matrixChanged(r, item); };
}

QList<QnUuid> QnVideoWallResource::onlineItems() const
{
    QList<QnUuid> result;
    for (const QnVideoWallItem &item : m_items->getItems())
    {
        if (!item.runtimeStatus.online)
            continue;
        result << item.uuid;
    }
    return result;
}

