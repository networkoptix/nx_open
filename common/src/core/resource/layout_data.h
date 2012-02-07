#ifndef _LAYOUT_DATA_H_
#define _LAYOUT_DATA_H_

#include <QSharedPointer>
#include <QRectF>
#include <QUuid>
#include "utils/common/qnid.h"
#include "core/resource/resource.h"

class QnLayoutItemData
{
public:
    QnId resourceId;
    QUuid uuid;
    int flags;
    QRectF combinedGeometry;
    qreal rotation;
};

typedef QList<QnLayoutItemData> QnLayoutItemDataList;

class QnLayoutData : public QnResource
{
    Q_OBJECT;

    typedef QnResource base_type;

public:
    QnLayoutData();

    virtual QString getUniqueId() const override;

    void setItems(const QnLayoutItemDataList& items);

    const QnLayoutItemDataList &getItems() const
    {
        return m_items;
    }

protected:
    virtual void updateInner(QnResourcePtr other) override;

private:
    QList<QnLayoutItemData> m_items;
};

typedef QSharedPointer<QnLayoutData> QnLayoutDataPtr;
typedef QList<QnLayoutDataPtr> QnLayoutDataList;

#endif // _LAYOUT_DATA_H_
