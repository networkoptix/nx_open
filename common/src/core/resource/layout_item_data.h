#pragma once

#include <QtCore/QHash>
#include <QtCore/QList>
#include <QtCore/QMetaType>
#include <nx/utils/uuid.h>
#include <QtCore/QVariant>

#include <core/ptz/item_dewarping_params.h>

#include <nx/utils/math/fuzzy.h>
#include <utils/common/id.h>
#include <utils/color_space/image_correction.h>

struct QnLayoutItemResourceDescriptor
{
    QnUuid id;
    QString uniqueId;
};

struct QnLayoutItemData
{
    QnLayoutItemData();

    QnLayoutItemResourceDescriptor resource;

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
