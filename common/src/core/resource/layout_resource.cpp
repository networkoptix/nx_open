#include "layout_resource.h"
#include <utils/common/warnings.h>

QnLayoutResource::QnLayoutResource(): base_type()
{
    addFlags(QnResource::layout);
}

void QnLayoutResource::setItems(const QnLayoutItemDataList& items)
{
    while(!m_itemByUuid.empty())
        removeItem(*m_itemByUuid.begin());

    foreach(const QnLayoutItemData &item, items)
        addItem(item);
}

QnLayoutItemDataList QnLayoutResource::getItems() const 
{
    return m_itemByUuid.values();
}

const QnLayoutItemDataMap &QnLayoutResource::getItemMap() const
{
    return m_itemByUuid;
}

QString QnLayoutResource::getUniqueId() const
{
    /* Actually it's not a unique id, as situation is possible when there is no id yet.
     * But QnResource requires us to define this method. */
    return getId().toString();
}

void QnLayoutResource::updateInner(QnResourcePtr other) 
{
    base_type::updateInner(other);

    QnLayoutResourcePtr localOther = other.dynamicCast<QnLayoutResource>();
    if(localOther)
        setItems(localOther->getItems());
}

void QnLayoutResource::addItem(const QnLayoutItemData &item) 
{
    if(m_itemByUuid.contains(item.uuid)) {
        qnWarning("Item with UUID %1 is already in this layout resource.", item.uuid.toString());
        return;
    }

    m_itemByUuid[item.uuid] = item;

    emit itemAdded(item);
}

void QnLayoutResource::removeItem(const QnLayoutItemData &item) 
{
    removeItem(item.uuid);
}

void QnLayoutResource::removeItem(const QUuid &itemUuid) 
{
    QnLayoutItemDataMap::iterator pos = m_itemByUuid.find(itemUuid);
    if(pos == m_itemByUuid.end())
        return;

    QnLayoutItemData item = *pos;
    m_itemByUuid.erase(pos);
    emit itemRemoved(item);
}