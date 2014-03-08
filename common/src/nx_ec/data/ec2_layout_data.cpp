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
        resource.resource.path = resourcePath;
        resource.zoomRect = QRectF(QPointF(zoomLeft, zoomTop), QPointF(zoomRight, zoomBottom));
        resource.zoomTargetUuid = zoomTargetUuid;
        resource.contrastParams = ImageCorrectionParams::deserialize(contrastParams);
        resource.dewarpingParams = QJson::deserialized<QnItemDewarpingParams>(dewarpingParams);
    }

    void ApiLayoutItemData::fromResource(const QnLayoutItemData& resource)
    {
        uuid = resource.uuid.toByteArray();
        flags = resource.flags;
        left = resource.combinedGeometry.topLeft().x();
        top = resource.combinedGeometry.topLeft().y();
        right = resource.combinedGeometry.bottomRight().x();
        bottom = resource.combinedGeometry.bottomRight().y();
        rotation = resource.rotation;
        resourceId = resource.resource.id;
        resourcePath = resource.resource.path;
        zoomLeft = resource.zoomRect.topLeft().x();
        zoomTop = resource.zoomRect.topLeft().y();
        zoomRight = resource.zoomRect.bottomRight().x();
        zoomBottom = resource.zoomRect.bottomRight().y();
        zoomTargetUuid = resource.zoomTargetUuid.toByteArray();
        contrastParams = resource.contrastParams.serialize();
        dewarpingParams = QJson::serialized(resource.dewarpingParams);
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

    void ApiLayoutData::fromResource(QnLayoutResourcePtr resource)
    {
        ApiResourceData::fromResource(resource);
        cellAspectRatio = resource->cellAspectRatio();
        cellSpacingWidth = resource->cellSpacing().width();
        cellSpacingHeight = resource->cellSpacing().height();
        userCanEdit = resource->userCanEdit();
        locked = resource->locked();
        backgroundImageFilename = resource->backgroundImageFilename();
        backgroundWidth = resource->backgroundSize().width();
        backgroundHeight = resource->backgroundSize().height();
        backgroundOpacity = resource->backgroundOpacity();
        const QnLayoutItemDataMap& layoutItems = resource->getItems();
        items.clear();
        items.reserve( layoutItems.size() );
        for( const QnLayoutItemData& item: layoutItems )
        {
            items.push_back( ApiLayoutItemData() );
            items.back().fromResource( item );
        }
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