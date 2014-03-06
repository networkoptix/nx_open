#include "videowall_resource.h"

//#include <core/resource_managment/resource_pool.h>
//#include <api/serializer/pb_serializer.h>
#include <utils/common/warnings.h>

QnVideoWallResource::QnVideoWallResource() :
    base_type()
{
    setStatus(Online, true);
    addFlags(QnResource::videowall | QnResource::remote);
}

QString QnVideoWallResource::getUniqueId() const
{
    return getGuid();
}

void QnVideoWallResource::updateInner(QnResourcePtr other) {
    base_type::updateInner(other);

    QnVideoWallResourcePtr localOther = other.dynamicCast<QnVideoWallResource>();
    if(localOther) {
        setItems(localOther->getItems());
        setPcs(localOther->getPcs());
    }
}

//------------ Item methods ------------------------------------------------

void QnVideoWallResource::setItems(const QnVideoWallItemList& items) {
    QnVideoWallItemMap map;
    foreach(const QnVideoWallItem &item, items)
        map[item.uuid] = item;
    setItems(map);
}

void QnVideoWallResource::setItems(const QnVideoWallItemMap &items) {
    QMutexLocker locker(&m_mutex);

    foreach(const QnVideoWallItem &item, m_itemByUuid)
        if(!items.contains(item.uuid))
            removeItemUnderLock(item.uuid);

    foreach(const QnVideoWallItem &item, items) {
        if(m_itemByUuid.contains(item.uuid)) {
            updateItemUnderLock(item.uuid, item);
        } else {
            addItemUnderLock(item);
        }
    }
}

QnVideoWallItemMap QnVideoWallResource::getItems() const {
    QMutexLocker locker(&m_mutex);

    return m_itemByUuid;
}

QnVideoWallItem QnVideoWallResource::getItem(const QUuid &itemUuid) const {
    QMutexLocker locker(&m_mutex);

    return m_itemByUuid.value(itemUuid);
}

bool QnVideoWallResource::hasItem(const QUuid &itemUuid) const {
    QMutexLocker locker(&m_mutex);

    return m_itemByUuid.contains(itemUuid);
}


void QnVideoWallResource::addItem(const QnVideoWallItem &item) {
    QMutexLocker locker(&m_mutex);
    addItemUnderLock(item);
}

void QnVideoWallResource::removeItem(const QnVideoWallItem &item) {
    removeItem(item.uuid);
}

void QnVideoWallResource::removeItem(const QUuid &itemUuid) {
    QMutexLocker locker(&m_mutex);
    removeItemUnderLock(itemUuid);
}

void QnVideoWallResource::updateItem(const QUuid &itemUuid, const QnVideoWallItem &item) {
    QMutexLocker locker(&m_mutex);
    updateItemUnderLock(itemUuid, item);
}

void QnVideoWallResource::addItemUnderLock(const QnVideoWallItem &item) {
    if(m_itemByUuid.contains(item.uuid)) {
        qnWarning("Item with UUID %1 is already in this videowall resource.", item.uuid.toString());
        return;
    }

    m_itemByUuid[item.uuid] = item;

    m_mutex.unlock();
    emit itemAdded(::toSharedPointer(this), item);
    m_mutex.lock();
}

void QnVideoWallResource::updateItemUnderLock(const QUuid &itemUuid, const QnVideoWallItem &item) {
    QnVideoWallItemMap::iterator pos = m_itemByUuid.find(itemUuid);
    if(pos == m_itemByUuid.end()) {
        qnWarning("There is no item with UUID %1 in this videowall.", itemUuid.toString());
        return;
    }

    if(*pos == item)
        return;

    *pos = item;

    m_mutex.unlock();
    emit itemChanged(::toSharedPointer(this), item);
    m_mutex.lock();
}

void QnVideoWallResource::removeItemUnderLock(const QUuid &itemUuid) {
    QnVideoWallItemMap::iterator pos = m_itemByUuid.find(itemUuid);
    if(pos == m_itemByUuid.end())
        return;

    QnVideoWallItem item = *pos;
    m_itemByUuid.erase(pos);

    m_mutex.unlock();
    emit itemRemoved(::toSharedPointer(this), item);
    m_mutex.lock();
}

//------------ Pc methods ------------------------------------------------

void QnVideoWallResource::setPcs(const QnVideoWallPcDataList& pcs) {
    QnVideoWallPcDataMap map;
    foreach(const QnVideoWallPcData &pc, pcs)
        map[pc.uuid] = pc;
    setPcs(map);
}

void QnVideoWallResource::setPcs(const QnVideoWallPcDataMap &pcs) {
    QMutexLocker locker(&m_mutex);

    foreach(const QnVideoWallPcData &pc, m_pcByUuid)
        if(!pcs.contains(pc.uuid))
            removePcUnderLock(pc.uuid);

    foreach(const QnVideoWallPcData &pc, pcs) {
        if(m_pcByUuid.contains(pc.uuid)) {
            updatePcUnderLock(pc.uuid, pc);
        } else {
            addPcUnderLock(pc);
        }
    }
}

QnVideoWallPcDataMap QnVideoWallResource::getPcs() const {
    QMutexLocker locker(&m_mutex);

    return m_pcByUuid;
}

QnVideoWallPcData QnVideoWallResource::getPc(const QUuid &pcUuid) const {
    QMutexLocker locker(&m_mutex);

    return m_pcByUuid.value(pcUuid);
}


void QnVideoWallResource::addPc(const QnVideoWallPcData &pc) {
    QMutexLocker locker(&m_mutex);
    addPcUnderLock(pc);
}

void QnVideoWallResource::removePc(const QnVideoWallPcData &pc) {
    removePc(pc.uuid);
}

void QnVideoWallResource::removePc(const QUuid &pcUuid) {
    QMutexLocker locker(&m_mutex);
    removePcUnderLock(pcUuid);
}

void QnVideoWallResource::updatePc(const QUuid &pcUuid, const QnVideoWallPcData &pc) {
    QMutexLocker locker(&m_mutex);
    updatePcUnderLock(pcUuid, pc);
}

void QnVideoWallResource::addPcUnderLock(const QnVideoWallPcData &pc) {
    if(m_pcByUuid.contains(pc.uuid)) {
        qnWarning("Pc with UUID %1 is already in this videowall resource.", pc.uuid.toString());
        return;
    }

    m_pcByUuid[pc.uuid] = pc;

    m_mutex.unlock();
    emit pcAdded(::toSharedPointer(this), pc);
    m_mutex.lock();
}

void QnVideoWallResource::updatePcUnderLock(const QUuid &pcUuid, const QnVideoWallPcData &pc) {
    QnVideoWallPcDataMap::iterator pos = m_pcByUuid.find(pcUuid);
    if(pos == m_pcByUuid.end()) {
        qnWarning("There is no pc with UUID %1 in this videowall.", pcUuid.toString());
        return;
    }

    if(*pos == pc)
        return;

    *pos = pc;

    m_mutex.unlock();
    emit pcChanged(::toSharedPointer(this), pc);
    m_mutex.lock();
}

void QnVideoWallResource::removePcUnderLock(const QUuid &pcUuid) {
    QnVideoWallPcDataMap::iterator pos = m_pcByUuid.find(pcUuid);
    if(pos == m_pcByUuid.end())
        return;

    QnVideoWallPcData pc = *pos;
    m_pcByUuid.erase(pos);

    m_mutex.unlock();
    emit pcRemoved(::toSharedPointer(this), pc);
    m_mutex.lock();
}
