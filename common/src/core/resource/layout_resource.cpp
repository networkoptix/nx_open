#include "layout_resource.h"

QnLayoutResource::QnLayoutResource(): base_type()
{
    addFlags(QnResource::layout);
}

void QnLayoutResource::setItems(const QnLayoutItemDataList& items)
{
    while(!m_items.empty())
        removeItem(m_items.front());

    foreach(const QnLayoutItemData &item, items)
        addItem(item);
}

const QnLayoutItemDataList &QnLayoutResource::getItems() const 
{
    return m_items;
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
    if(localOther) {
        setItems(localOther->getItems());
    }
}

void QnLayoutResource::addItem(const QnLayoutItemData &item) 
{
    m_items.push_back(item);

    emit itemAdded(item);
}

void QnLayoutResource::removeItem(const QnLayoutItemData &item) 
{
    removeItem(item.uuid);
}

void QnLayoutResource::removeItem(const QUuid &itemUuid) 
{
    int index;
    for(index = 0; index < m_items.size(); index++)
        if(m_items[index].uuid == itemUuid)
            break;
    if(index >= m_items.size())
        return;

    QnLayoutItemData item = m_items[index];
    m_items.removeAt(index);
    emit itemRemoved(item);
}