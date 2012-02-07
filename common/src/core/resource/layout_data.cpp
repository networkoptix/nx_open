#include "layout_data.h"

QnLayoutData::QnLayoutData(): base_type()
{
    addFlag(QnResource::layout);
}

void QnLayoutData::setItems(const QnLayoutItemDataList& items)
{
    m_items = items;
}

QString QnLayoutData::getUniqueId() const
{
    // Actually it's not a unique id, as situation is possible
    // when there is no id yet.
    // But QnResource requires us to define this method.
    return getId().toString();
}

void QnLayoutData::updateInner(QnResourcePtr other) {
    base_type::updateInner(other);

    QnLayoutDataPtr localOther = other.dynamicCast<QnLayoutData>();
    if(localOther) {
        setItems(localOther->getItems());
    }
}

