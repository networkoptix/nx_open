#include "layout_resource.h"

QnLayoutResource::QnLayoutResource(): base_type()
{
    addFlag(QnResource::layout);
}

void QnLayoutResource::setItems(const QnLayoutItemDataList& items)
{
    m_items = items;
}

QString QnLayoutResource::getUniqueId() const
{
    // Actually it's not a unique id, as situation is possible
    // when there is no id yet.
    // But QnResource requires us to define this method.
    return getId().toString();
}

void QnLayoutResource::updateInner(QnResourcePtr other) {
    base_type::updateInner(other);

    QnLayoutResourcePtr localOther = other.dynamicCast<QnLayoutResource>();
    if(localOther) {
        setItems(localOther->getItems());
    }
}

