#include "layout_resource.h"
#include <utils/common/warnings.h>

QnLayoutResource::QnLayoutResource(): base_type() {
    addFlags(QnResource::layout);
}

void QnLayoutResource::setItems(const QnLayoutItemDataList& items) {
    QnLayoutItemDataMap map;
    foreach(const QnLayoutItemData &item, items)
        map[item.uuid] = item;
    setItems(map);
}

void QnLayoutResource::setItems(const QnLayoutItemDataMap &items) {
    QMutexLocker locker(&m_mutex);

    foreach(const QnLayoutItemData &item, m_itemByUuid)
        if(!items.contains(item.uuid))
            removeItem(item);

    foreach(const QnLayoutItemData &item, items) {
        if(m_itemByUuid.contains(item.uuid)) {
            updateItem(item.uuid, item);
        } else {
            addItem(item);
        }
    }
}

QnLayoutItemDataMap QnLayoutResource::getItems() const {
    QMutexLocker locker(&m_mutex);

    return m_itemByUuid;
}

QnLayoutItemData QnLayoutResource::getItem(const QUuid &itemUuid) const {
    QMutexLocker locker(&m_mutex);

    return m_itemByUuid.value(itemUuid);
}

QString QnLayoutResource::getUniqueId() const
{
    /* Actually it's not a unique id, as situation is possible when there is no id yet.
     * But QnResource requires us to define this method. */
    return getId().toString();
}

void QnLayoutResource::updateInner(QnResourcePtr other) {
    base_type::updateInner(other);

    QnLayoutResourcePtr localOther = other.dynamicCast<QnLayoutResource>();
    if(localOther)
        setItems(localOther->getItems());
}

void QnLayoutResource::addItem(const QnLayoutItemData &item) {
    QMutexLocker locker(&m_mutex);

    if(m_itemByUuid.contains(item.uuid)) {
        qnWarning("Item with UUID %1 is already in this layout resource.", item.uuid.toString());
        return;
    }

    m_itemByUuid[item.uuid] = item;

    emit itemAdded(item);
}

void QnLayoutResource::removeItem(const QnLayoutItemData &item) {
    removeItem(item.uuid);
}

void QnLayoutResource::removeItem(const QUuid &itemUuid) {
    QMutexLocker locker(&m_mutex);

    QnLayoutItemDataMap::iterator pos = m_itemByUuid.find(itemUuid);
    if(pos == m_itemByUuid.end())
        return;

    QnLayoutItemData item = *pos;
    m_itemByUuid.erase(pos);
    emit itemRemoved(item);
}

void QnLayoutResource::updateItem(const QUuid &itemUuid, const QnLayoutItemData &item) {
    QMutexLocker locker(&m_mutex);

    QnLayoutItemDataMap::iterator pos = m_itemByUuid.find(itemUuid);
    if(pos == m_itemByUuid.end()) {
        qnWarning("There is no item with UUID %1 in this layout.", itemUuid.toString());
        return;
    }

    if(*pos == item)
        return;

    *pos = item;

    emit itemChanged(item);
}
