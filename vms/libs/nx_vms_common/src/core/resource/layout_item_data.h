// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QHash>
#include <QtCore/QList>
#include <QtCore/QMetaType>
#include <QtCore/QVariant>
#include <QtCore/QRect>

#include <nx/vms/api/data/dewarping_data.h>
#include <nx/vms/api/data/image_correction_data.h>

#include <nx/utils/uuid.h>

#include "resource_fwd.h"

struct QnLayoutItemResourceDescriptor
{
    QnUuid id;
    QString uniqueId;
};

struct NX_VMS_COMMON_API QnLayoutItemData
{
    QnLayoutItemResourceDescriptor resource;

    QnUuid uuid;
    int flags = 0;
    QRectF combinedGeometry;
    QnUuid zoomTargetUuid;
    QRectF zoomRect;
    qreal rotation = 0.0;
    bool displayInfo = false;
    bool controlPtz = false;
    bool displayAnalyticsObjects = false;
    bool displayRoi = true;

    nx::vms::api::ImageCorrectionData contrastParams;
    nx::vms::api::dewarping::ViewData dewarpingParams;

    bool operator==(const QnLayoutItemData& other) const;

    static QnLayoutItemData createFromResource(const QnResourcePtr& resource);
};

Q_DECLARE_METATYPE(QnLayoutItemData);
Q_DECLARE_TYPEINFO(QnLayoutItemData, Q_MOVABLE_TYPE);

typedef QList<QnLayoutItemData> QnLayoutItemDataList;
typedef QHash<QnUuid, QnLayoutItemData> QnLayoutItemDataMap;

Q_DECLARE_METATYPE(QnLayoutItemDataList);
Q_DECLARE_METATYPE(QnLayoutItemDataMap);
