#pragma once

#include "resource_data.h"

#include <QtCore/QString>
#include <QtCore/QtGlobal>

#include "dewarping_data.h"
#include "image_correction_data.h"

namespace nx::vms::api {

struct NX_VMS_API LayoutItemData: IdData
{
    qint32 flags = 0;
    float left = 0;
    float top = 0;
    float right = 0;
    float bottom = 0;
    float rotation = 0;
    QnUuid resourceId;
    QString resourcePath;
    float zoomLeft = 0;
    float zoomTop = 0;
    float zoomRight = 0;
    float zoomBottom = 0;
    QnUuid zoomTargetId;
    ImageCorrectionData contrastParams;
    DewarpingData dewarpingParams;
    bool displayInfo = false; /**< Should info be displayed on the item. */
};
#define LayoutItemData_Fields IdData_Fields (flags)(left)(top)(right)(bottom)(rotation) \
    (resourceId)(resourcePath)(zoomLeft)(zoomTop)(zoomRight)(zoomBottom)(zoomTargetId) \
    (contrastParams)(dewarpingParams)(displayInfo)

struct NX_VMS_API LayoutData: ResourceData
{
    LayoutData(): ResourceData(kResourceTypeId) {}

    static const QString kResourceTypeName;
    static const QnUuid kResourceTypeId;

    static constexpr float kDefaultCellSpacing = 0.05f;
    static constexpr float kDefaultBackgroundOpacity = 0.7f;

    float cellAspectRatio = 0;
    float horizontalSpacing = kDefaultCellSpacing; //< TODO: #API Rename to cellSpacing.
    float verticalSpacing = kDefaultCellSpacing; //< TODO: #API Remove this one.
    LayoutItemDataList items;
    bool locked = false;
    qint32 fixedWidth = 0;
    qint32 fixedHeight = 0;
    qint32 logicalId = 0;
    QString backgroundImageFilename;
    qint32 backgroundWidth = 0;
    qint32 backgroundHeight = 0;
    float backgroundOpacity = kDefaultBackgroundOpacity;
};
#define LayoutData_Fields ResourceData_Fields (cellAspectRatio)(horizontalSpacing) \
    (verticalSpacing)(items)(locked)(backgroundImageFilename)(backgroundWidth) \
    (backgroundHeight)(backgroundOpacity) \
    (fixedWidth)(fixedHeight)(logicalId)

} // namespace nx::vms::api

Q_DECLARE_METATYPE(nx::vms::api::LayoutItemData)
Q_DECLARE_METATYPE(nx::vms::api::LayoutItemDataList)
Q_DECLARE_METATYPE(nx::vms::api::LayoutData)
Q_DECLARE_METATYPE(nx::vms::api::LayoutDataList)
