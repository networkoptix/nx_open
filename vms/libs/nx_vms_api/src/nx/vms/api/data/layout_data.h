// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>
#include <QtCore/QtGlobal>

#include <nx/reflect/instrument.h>

#include "dewarping_data.h"
#include "id_data.h"
#include "image_correction_data.h"
#include "resource_data.h"

namespace nx::vms::api {

/**%apidoc
 * Layout Item information object
 * %param [immutable] id Item unique id.
 */
struct NX_VMS_API LayoutItemData: IdData
{
    /**%apidoc [opt] Should have fixed value.*/
    qint32 flags = 0;

    /**%apidoc Left coordinate of the layout item (floating-point).
     * %example 0
     */
    float left = 0;

    /**%apidoc Top coordinate of the layout item (floating-point).
     * %example 0
     */
    float top = 0;

    /**%apidoc Right coordinate of the layout item (floating-point).
     * %example 1
     */
    float right = 0;

    /**%apidoc Bottom coordinate of the layout item (floating-point).
     * %example 1
     */
    float bottom = 0;

    /**%apidoc [opt] Degree of image tilt; a positive value rotates counter-clockwise
     * (floating-point, 0..360).
     */
    float rotation = 0;

    /**%apidoc Device unique id.*/
    QnUuid resourceId;

    /**%apidoc [opt] If the item represents a local file - the URL of the file. For cross-Site
     * layouts, contains the special URL of the Device. Otherwise is empty. Can be filled with the
     * Device logical id when saving the Layout.
     */
    QString resourcePath;

    /**%apidoc [opt] Left coordinate of the displayed window inside the camera image, as a fraction
     * of the image width (floating-point, 0..1).
     */
    float zoomLeft = 0;

    /**%apidoc [opt] Top coordinate of the displayed window inside the camera image, as a fraction
     * of the image height (floating-point, 0..1).
     */
    float zoomTop = 0;

    /**%apidoc [opt] Right coordinate of the displayed window inside the camera image, as a
     * fraction of the image width (floating-point, 0..1).
     */
    float zoomRight = 0;

    /**%apidoc [opt] Bottom coordinate of the displayed window inside the camera image, as a
     * fraction of the image height (floating-point, 0..1).
     */
    float zoomBottom = 0;

    /**%apidoc [opt] Unique id of the original layout item for which the zoom window was created.*/
    QnUuid zoomTargetId;

    /**%apidoc [opt] Image enhancement parameters.*/
    ImageCorrectionData contrastParams;

    /**%apidoc [opt] Image dewarping parameters.*/
    nx::vms::api::dewarping::ViewData dewarpingParams;

    /**%apidoc [opt] Whether to display info for the layout item.*/
    bool displayInfo = false;

    /**%apidoc [opt] Whether PTZ control is enabled for the layout item.*/
    bool controlPtz = false;

    /**%apidoc [opt] Should the detected analytics objects be displayed on the item.*/
    bool displayAnalyticsObjects = false;

    /**%apidoc [opt] Should the regions of interest be displayed on the item.*/
    bool displayRoi = true;

    /**%apidoc [opt] Whether camera hotspots are enabled on the item.*/
    bool displayHotspots = false;

    bool operator==(const LayoutItemData& other) const;
};
#define LayoutItemData_Fields IdData_Fields (flags)(left)(top)(right)(bottom)(rotation) \
    (resourceId)(resourcePath)(zoomLeft)(zoomTop)(zoomRight)(zoomBottom)(zoomTargetId) \
    (contrastParams)(dewarpingParams)(displayInfo)(displayAnalyticsObjects)(displayRoi) \
    (controlPtz)(displayHotspots)
NX_VMS_API_DECLARE_STRUCT_AND_LIST(LayoutItemData)
NX_REFLECTION_INSTRUMENT(LayoutItemData, LayoutItemData_Fields)

/**%apidoc Layout information object
 * %param [immutable] id Layout unique id.
 * %param [opt] parentId Unique id of the user owning the layout.
 * %param name Layout name.
 *     %example Layout
 * %param [unused] url
 * %param [unused] typeId
 */
struct NX_VMS_API LayoutData: ResourceData
{
    LayoutData(): ResourceData(kResourceTypeId) {}
    bool operator==(const LayoutData& other) const;

    static const QString kResourceTypeName;
    static const QnUuid kResourceTypeId;

    static constexpr float kDefaultCellAspectRatio = 16.0f / 9.0f;
    static constexpr float kDefaultCellSpacing = 0.05f;
    static constexpr float kDefaultBackgroundOpacity = 0.7f;

    /**%apidoc [opt] Aspect ratio of a cell for layout items (floating-point).*/
    float cellAspectRatio = 0;
    /**%apidoc [opt] Cell spacing between layout items as a share of an item's size
     * (floating-point, 0..1).
     */
    float cellSpacing = kDefaultCellSpacing;
    /**%apidoc List of the layout items.*/
    LayoutItemDataList items;
    /**%apidoc [opt] Whether the layout is locked.*/
    bool locked = false;
    /**%apidoc Fixed width of the layout in cells (integer).
     * %example 1
     */
    qint32 fixedWidth = 0;
    /**%apidoc Fixed height of the layout in cells (integer).
     * %example 1
     */
    qint32 fixedHeight = 0;
    /**%apidoc [opt] Logical id of the layout, set by user (integer).*/
    qint32 logicalId = 0;
    /**%apidoc [opt] Background image filename.*/
    QString backgroundImageFilename;
    /**%apidoc [opt] Width of the background image in cells (integer).*/
    qint32 backgroundWidth = 0;
    /**%apidoc [opt] Height of the background image in cells (integer).*/
    qint32 backgroundHeight = 0;
    /**%apidoc [opt] Level of opacity of the background image (floating-point, 0..1).*/
    float backgroundOpacity = kDefaultBackgroundOpacity;
};
#define LayoutData_Fields ResourceData_Fields (cellAspectRatio)(cellSpacing) \
    (items)(locked)(backgroundImageFilename)(backgroundWidth) \
    (backgroundHeight)(backgroundOpacity) \
    (fixedWidth)(fixedHeight)(logicalId)
NX_VMS_API_DECLARE_STRUCT_AND_LIST(LayoutData)
NX_REFLECTION_INSTRUMENT(LayoutData, LayoutData_Fields)

class NX_VMS_API LayoutItemId: public QnUuid
{
public:
    LayoutItemId() = default;
    LayoutItemId(const QnUuid& id, const QnUuid& layoutId): QnUuid(id), layoutId(layoutId) {}
    QnUuid layoutId;
};

/**%apidoc
 * Layout Item filter
 * %param id Layout Item unique id.
 */
struct NX_VMS_API LayoutItemFilter: IdData
{
    LayoutItemFilter() = default;
    LayoutItemFilter(const LayoutItemId& id): IdData(id), layoutId(id.layoutId) {}

    /**%apidoc Layout unique id.*/
    QnUuid layoutId;
};
#define LayoutItemFilter_Fields (id)(layoutId)
QN_FUSION_DECLARE_FUNCTIONS(LayoutItemFilter, (json), NX_VMS_API)

struct NX_VMS_API LayoutItemModel: LayoutItemData
{
    /**%apidoc [readonly] Layout unique id.*/
    QnUuid layoutId;

    LayoutItemModel() = default;
    LayoutItemModel(LayoutItemData data, QnUuid layoutId_);

    LayoutItemId getId() const { return {id, layoutId}; }
};
#define LayoutItemModel_Fields LayoutItemData_Fields (layoutId)
QN_FUSION_DECLARE_FUNCTIONS(LayoutItemModel, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(LayoutItemModel, LayoutItemModel_Fields)

} // namespace nx::vms::api
