#ifndef _LAYOUT_DATA_H_
#define _LAYOUT_DATA_H_

#include <QSharedPointer>
#include <QRectF>

#include "core/resource/resource.h"

struct QnLayoutItemData
{
    int flags;
    QRectF combinedGeometry;
    qreal rotation;
};

typedef QList<QnLayoutItemData> QnLayoutItemDataList;

class QnLayoutData : public QnResource
{
public:
    QString getUniqueId() const
    {
        // Indeed it's not uniqueid, as there is possible situation
        // when there is no id yet.
        // But QnResource requires us to define this method.
        return getId().toString();
    }

    void setItems(const QnLayoutItemDataList& items)
    {
        m_items = items;
    }

    QnLayoutItemDataList getItems() const
    {
        return m_items;
    }
private:
    QList<QnLayoutItemData> m_items;
};

typedef QSharedPointer<QnLayoutData> QnLayoutDataPtr;
typedef QList<QnLayoutDataPtr> QnLayoutDataList;

#endif // _LAYOUT_DATA_H_
