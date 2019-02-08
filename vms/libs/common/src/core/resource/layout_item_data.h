#pragma once

#include <QtCore/QHash>
#include <QtCore/QList>
#include <QtCore/QMetaType>
#include <QtCore/QVariant>

#include <nx/vms/api/data/dewarping_data.h>
#include <nx/vms/api/data/image_correction_data.h>

#include <nx/utils/uuid.h>

struct QnLayoutItemResourceDescriptor
{
    QnUuid id;
    QString uniqueId;
};

struct QnLayoutItemData
{
    QnLayoutItemResourceDescriptor resource;

    QnUuid uuid;
    int flags = 0;
    QRectF combinedGeometry;
    QnUuid zoomTargetUuid;
    QRectF zoomRect;
    qreal rotation = 0.0;
    bool displayInfo = false;

    nx::vms::api::ImageCorrectionData contrastParams;
    nx::vms::api::DewarpingData dewarpingParams;

    friend bool operator==(const QnLayoutItemData &l, const QnLayoutItemData &r) ;
};

Q_DECLARE_METATYPE(QnLayoutItemData);
Q_DECLARE_TYPEINFO(QnLayoutItemData, Q_MOVABLE_TYPE);

typedef QList<QnLayoutItemData> QnLayoutItemDataList;
typedef QHash<QnUuid, QnLayoutItemData> QnLayoutItemDataMap;

Q_DECLARE_METATYPE(QnLayoutItemDataList);
Q_DECLARE_METATYPE(QnLayoutItemDataMap);
