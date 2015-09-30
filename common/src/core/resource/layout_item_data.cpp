#include "layout_item_data.h"


QnLayoutItemData::QnLayoutItemData() :
      uuid()
    , flags(0)
    , combinedGeometry()
    , zoomTargetUuid()
    , zoomRect()
    , rotation(0.0)
    , displayInfo(false)
    , contrastParams()
    , dewarpingParams()
    , dataByRole()
{}

bool operator==(const QnLayoutItemData &l, const QnLayoutItemData &r) {
    /* Checking all fields but dataByRole. */
    return (
           l.uuid == r.uuid 
        && l.resource.id == r.resource.id
        && l.resource.path == r.resource.path
        && l.flags == r.flags 
        && qFuzzyEquals(l.combinedGeometry, r.combinedGeometry)
        && l.zoomTargetUuid == r.zoomTargetUuid 
        && qFuzzyEquals(l.zoomRect, r.zoomRect)
        && qFuzzyEquals(l.rotation, r.rotation)
        && l.displayInfo == r.displayInfo
        && l.contrastParams == r.contrastParams
        && l.dewarpingParams == r.dewarpingParams
        );
}
