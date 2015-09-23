#ifndef QN_LAYOUT_ITEM_DATA_H
#define QN_LAYOUT_ITEM_DATA_H

#include <QtCore/QHash>
#include <QtCore/QList>
#include <QtCore/QMetaType>
#include <utils/common/uuid.h>
#include <QtCore/QVariant>

#include <core/ptz/item_dewarping_params.h>

#include <utils/math/fuzzy.h>
#include <utils/common/id.h>
#include <utils/color_space/image_correction.h>

class QnLayoutItemData {
public:
    QnLayoutItemData();

    struct {
        QnUuid id;
        QString path;
    } resource;

    QnUuid uuid;
    int flags;
    QRectF combinedGeometry;
    QnUuid zoomTargetUuid;
    QRectF zoomRect;
    qreal rotation;
    bool displayInfo;

    ImageCorrectionParams contrastParams;
    QnItemDewarpingParams dewarpingParams;

    QHash<int, QVariant> dataByRole;

    friend bool operator==(const QnLayoutItemData &l, const QnLayoutItemData &r) ;
};

Q_DECLARE_METATYPE(QnLayoutItemData);
Q_DECLARE_TYPEINFO(QnLayoutItemData, Q_MOVABLE_TYPE);

typedef QList<QnLayoutItemData> QnLayoutItemDataList;
typedef QHash<QnUuid, QnLayoutItemData> QnLayoutItemDataMap;

Q_DECLARE_METATYPE(QnLayoutItemDataList);
Q_DECLARE_METATYPE(QnLayoutItemDataMap);

#endif // QN_LAYOUT_ITEM_DATA_H
