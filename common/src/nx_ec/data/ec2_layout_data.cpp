#include "ec2_layout_data.h"
#include "utils/common/json_serializer.h"
#include "core/resource/layout_resource.h"

namespace ec2
{

    void ApiLayoutItemData::toResource(QnLayoutItemData& resource) const
    {
        resource.uuid = uuid;
        resource.flags = flags;
        resource.combinedGeometry = QRectF(QPointF(left, top), QPointF(right, bottom));
        resource.rotation = rotation;
        resource.resource.id =  resourceId;
        resource.zoomRect = QRectF(QPointF(zoomLeft, zoomTop), QPointF(zoomRight, zoomBottom));
        resource.zoomTargetUuid = zoomTargetUuid;
        resource.contrastParams = ImageCorrectionParams::deserialize(contrastParams);
        resource.dewarpingParams = QJson::deserialized<QnItemDewarpingParams>(dewarpingParams);
    }

    void ApiLayoutData::toResource(QnLayoutResourcePtr resource) const
    {
        ApiResourceData::toResource(resource);
        resource->setCellAspectRatio(cellAspectRatio);
        resource->setCellSpacing(cellSpacingWidth, cellSpacingHeight);
        resource->setUserCanEdit(userCanEdit);
        resource->setLocked(locked);
        resource->setBackgroundImageFilename(backgroundImageFilename);
        resource->setBackgroundSize(QSize(backgroundWidth, backgroundHeight));
        resource->setBackgroundOpacity(backgroundOpacity);
        QnLayoutItemDataList outItems;
        for (int i = 0; i < items.size(); ++i) {
            outItems << QnLayoutItemData();
            items[i].toResource(outItems.last());
        }
        resource->setItems(outItems);
    }

    void ApiLayoutDataList::toLayoutList(QnLayoutResourceList& outData) const
    {
        outData.reserve(data.size());
        for(int i = 0; i < data.size(); ++i) 
        {
            QnLayoutResourcePtr layout(new QnLayoutResource());
            data[i].toResource(layout);
            outData << layout;
        }
    }

    void ApiLayoutDataList::loadFromQuery(QSqlQuery& query)
    {
        QN_QUERY_TO_DATA_OBJECT(query, ApiLayoutData, data, ApiLayoutDataFields ApiResourceDataFields)
    }

}