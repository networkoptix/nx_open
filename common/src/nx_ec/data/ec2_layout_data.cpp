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

    template <class T>
    void ApiLayoutDataList::toResourceList(QList<T>& outData) const
    {
        outData.reserve(outData.size() + data.size());
        for(int i = 0; i < data.size(); ++i) 
        {
            QnLayoutResourcePtr layout(new QnLayoutResource());
            data[i].toResource(layout);
            outData << layout;
        }
    }
    template void ApiLayoutDataList::toResourceList<QnResourcePtr>(QList<QnResourcePtr>& outData) const;
    template void ApiLayoutDataList::toResourceList<QnLayoutResourcePtr>(QList<QnLayoutResourcePtr>& outData) const;

    void ApiLayoutDataList::loadFromQuery(QSqlQuery& query)
    {
        QN_QUERY_TO_DATA_OBJECT(query, ApiLayoutData, data, ApiLayoutDataFields ApiResourceDataFields)
    }

}