// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QHash>
#include <QtCore/QList>
#include <QtCore/QRect>
#include <QtCore/QVariant>
#include <QtGui/QColor>

#include <nx/reflect/instrument.h>
#include <nx/utils/uuid.h>
#include <nx/vms/api/data/dewarping_data.h>
#include <nx/vms/api/data/image_correction_data.h>
#include <nx/vms/common/resource/resource_descriptor.h>

namespace nx::vms::common {

struct NX_VMS_COMMON_API LayoutItemData
{
    nx::vms::common::ResourceDescriptor resource;

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
    QColor frameDistinctionColor;
    nx::vms::api::ImageCorrectionData contrastParams;
    nx::vms::api::dewarping::ViewData dewarpingParams;
    bool displayHotspots = false;

    bool operator==(const LayoutItemData& other) const;
};

NX_REFLECTION_INSTRUMENT(LayoutItemData,
    (resource)
    (uuid)
    (flags)
    (combinedGeometry)
    (zoomTargetUuid)
    (zoomRect)
    (rotation)
    (displayInfo)
    (controlPtz)
    (displayAnalyticsObjects)
    (displayRoi)
    (frameDistinctionColor)
    (contrastParams)
    (dewarpingParams)
    (displayHotspots))

using LayoutItemDataList = QList<LayoutItemData>;
using LayoutItemDataMap = QHash<QnUuid, LayoutItemData>;

} // namespace nx::vms::common
