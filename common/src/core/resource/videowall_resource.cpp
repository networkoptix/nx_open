#include "videowall_resource.h"

QnVideoWallResource::QnVideoWallResource() :
    base_type(),
    m_autorun(false),
    m_items(new QnResourceItemStorage<QnVideoWallItem>(&m_mutex, this)),
    m_pcs(new QnResourceItemStorage<QnVideoWallPcData>(&m_mutex, this)),
    m_matrices(new QnResourceItemStorage<QnVideoWallMatrix>(&m_mutex, this))
{
    setStatus(Online, true);
    addFlags(QnResource::videowall | QnResource::remote);
}

QnResourceItemStorage<QnVideoWallItem> * QnVideoWallResource::items() const {
    return m_items.data();
}

QnResourceItemStorage<QnVideoWallPcData> * QnVideoWallResource::pcs() const {
    return m_pcs.data();
}

QnResourceItemStorage<QnVideoWallMatrix> * QnVideoWallResource::matrices() const {
    return m_matrices.data();
}

void QnVideoWallResource::updateInner(const QnResourcePtr &other, QSet<QByteArray>& modifiedFields) {
    base_type::updateInner(other, modifiedFields);

    QnVideoWallResource* localOther = dynamic_cast<QnVideoWallResource*>(other.data());
    if(localOther) {
        m_items->setItemsUnderLock(localOther->items());
        m_pcs->setItemsUnderLock(localOther->pcs());
        m_matrices->setItemsUnderLock(localOther->matrices());
        if (m_autorun != localOther->m_autorun) {
            m_autorun = localOther->m_autorun;
            modifiedFields << "autorunChanged";
        }
    }
}

bool QnVideoWallResource::isAutorun() const {
    QMutexLocker locker(&m_mutex);
    return m_autorun;
}

void QnVideoWallResource::setAutorun(bool value) {
    {
        QMutexLocker locker(&m_mutex);
        if (m_autorun == value)
            return;
        m_autorun = value;
    }
    emit autorunChanged(::toSharedPointer(this));
}

void QnVideoWallResource::storedItemAdded(const QnVideoWallItem &item) {
    emit itemAdded(::toSharedPointer(this), item);
}

void QnVideoWallResource::storedItemRemoved(const QnVideoWallItem &item) {
    emit itemRemoved(::toSharedPointer(this), item);

    // removing item from the matrices and removing empty matrices (if any)
    QnVideoWallMatrixMap matrices = m_matrices->getItems();
    foreach (QnVideoWallMatrix matrix, matrices) {
        if (!matrix.layoutByItem.contains(item.uuid))
            continue;
        matrix.layoutByItem.remove(item.uuid);
        if (matrix.layoutByItem.isEmpty())
            m_matrices->removeItem(matrix.uuid);
        else
            m_matrices->updateItem(matrix.uuid, matrix);
    }

    QUuid pcUuid = item.pcUuid;
    foreach(const QnVideoWallItem &item, m_items->getItems())
        if (item.pcUuid == pcUuid)
            return;

    //removed last item from this pc
    m_pcs->removeItem(pcUuid);
}

void QnVideoWallResource::storedItemChanged(const QnVideoWallItem &item) {
    emit itemChanged(::toSharedPointer(this), item);
}

void QnVideoWallResource::storedItemAdded(const QnVideoWallPcData &item) {
    emit pcAdded(::toSharedPointer(this), item);
}

void QnVideoWallResource::storedItemRemoved(const QnVideoWallPcData &item) {
    emit pcRemoved(::toSharedPointer(this), item);
}

void QnVideoWallResource::storedItemChanged(const QnVideoWallPcData &item) {
    emit pcChanged(::toSharedPointer(this), item);
}

void QnVideoWallResource::storedItemAdded(const QnVideoWallMatrix &item) {
    emit matrixAdded(::toSharedPointer(this), item);
}

void QnVideoWallResource::storedItemRemoved(const QnVideoWallMatrix &item) {
    emit matrixRemoved(::toSharedPointer(this), item);
}

void QnVideoWallResource::storedItemChanged(const QnVideoWallMatrix &item) {
    emit matrixChanged(::toSharedPointer(this), item);
}

QList<QUuid> QnVideoWallResource::onlineItems() const {
    QList<QUuid> result;
    foreach (const QnVideoWallItem &item, m_items->getItems()) {
        if (!item.online)
            continue;
        result << item.uuid;
    }
    return result;
}
