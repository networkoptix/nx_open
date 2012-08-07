#ifndef QN_LAYOUT_ITEM_DATA_H
#define QN_LAYOUT_ITEM_DATA_H

#include <QtCore/QMetaType>
#include <QtCore/QUuid>
#include <QtCore/QList>
#include <QtCore/QHash>

#include <utils/common/fuzzy.h>
#include <utils/common/qnid.h>

class QnLayoutItemData {
public:
    QnLayoutItemData(): flags(0), rotation(0) {}

    struct {
        QnId id;
        QString path;
    } resource;

    QUuid uuid;
    int flags;
    QRectF combinedGeometry;
    qreal rotation;

    friend bool operator==(const QnLayoutItemData &l, const QnLayoutItemData &r) {
        if (l.uuid != r.uuid || l.flags != r.flags || !qFuzzyCompare(l.combinedGeometry, r.combinedGeometry) || !qFuzzyCompare(l.rotation, r.rotation))
            return false;

        if(l.resource.path == r.resource.path && (l.resource.id == r.resource.id || !l.resource.id.isValid() || !r.resource.id.isValid()))
            return true;

        if(l.resource.id == r.resource.id && (l.resource.path == r.resource.path || l.resource.path.isEmpty() || r.resource.path.isEmpty()))
            return true;

        return false;
    }
};

Q_DECLARE_METATYPE(QnLayoutItemData);
Q_DECLARE_TYPEINFO(QnLayoutItemData, Q_MOVABLE_TYPE);

typedef QList<QnLayoutItemData> QnLayoutItemDataList;
typedef QHash<QUuid, QnLayoutItemData> QnLayoutItemDataMap;

Q_DECLARE_METATYPE(QnLayoutItemDataList);
Q_DECLARE_METATYPE(QnLayoutItemDataMap);

#endif // QN_LAYOUT_ITEM_DATA_H
