#ifndef _LAYOUT_DATA_H_
#define _LAYOUT_DATA_H_

#include <QSharedPointer>
#include <QRectF>

struct QnLayoutItemData
{
    int flags;
    QRectF combinedGeometry;
    qreal rotation;
};

struct QnLayoutData
{
    QString id;
    QString name;
    QList<QnLayoutItemData> items;
};

typedef QSharedPointer<QnLayoutData> QnLayoutDataPtr;
typedef QList<QnLayoutDataPtr> QnLayoutDataList;

#endif // _LAYOUT_DATA_H_
