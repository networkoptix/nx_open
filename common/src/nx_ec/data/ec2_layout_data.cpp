#include "ec2_layout_data.h"
#include "utils/serialization/json.h"
#include "core/resource/layout_resource.h"

//QN_DEFINE_DERIVED_STRUCT_SERIALIZATORS(ApiLayoutData, ApiResourceData, ApiLayoutFields);
QN_FUSION_DECLARE_FUNCTIONS(ApiLayoutData, (binary))

    QN_DEFINE_API_OBJECT_LIST_DATA(ApiLayoutData);

//QN_DEFINE_STRUCT_SERIALIZATORS(ApiLayoutItemData, ApiLayoutItemFields);
QN_FUSION_DECLARE_FUNCTIONS(ApiLayoutItemData, (binary))


namespace ec2
{

    void fromApiToResource(const ApiLayoutItemData& data, QnLayoutItemData& resource)
    {
        resource.uuid = data.uuid;
        resource.flags = data.flags;
        resource.combinedGeometry = QRectF(QPointF(data.left, data.top), QPointF(data.right, data.bottom));
        resource.rotation = data.rotation;
        resource.resource.id =  data.resourceId;
        resource.resource.path = data.resourcePath;
        resource.zoomRect = QRectF(QPointF(data.zoomLeft, data.zoomTop), QPointF(data.zoomRight, data.zoomBottom));
        resource.zoomTargetUuid = data.zoomTargetUuid;
        resource.contrastParams = ImageCorrectionParams::deserialize(data.contrastParams);
        resource.dewarpingParams = QJson::deserialized<QnItemDewarpingParams>(data.dewarpingParams);
    }

    void fromResourceToApi(const QnLayoutItemData& resource, ApiLayoutItemData& data)
    {
        data.uuid = resource.uuid.toByteArray();
        data.flags = resource.flags;
        data.left = resource.combinedGeometry.topLeft().x();
        data.top = resource.combinedGeometry.topLeft().y();
        data.right = resource.combinedGeometry.bottomRight().x();
        data.bottom = resource.combinedGeometry.bottomRight().y();
        data.rotation = resource.rotation;
        data.resourceId = resource.resource.id;
        data.resourcePath = resource.resource.path;
        data.zoomLeft = resource.zoomRect.topLeft().x();
        data.zoomTop = resource.zoomRect.topLeft().y();
        data.zoomRight = resource.zoomRect.bottomRight().x();
        data.zoomBottom = resource.zoomRect.bottomRight().y();
        data.zoomTargetUuid = resource.zoomTargetUuid.toByteArray();
        data.contrastParams = resource.contrastParams.serialize();
        data.dewarpingParams = QJson::serialized(resource.dewarpingParams);
    }


    void fromApiToResource(const ApiLayoutData& data, QnLayoutResourcePtr resource)
    {
        fromApiToResource((const ApiResourceData &)data, resource);
        resource->setCellAspectRatio(data.cellAspectRatio);
        resource->setCellSpacing(data.cellSpacingWidth, data.cellSpacingHeight);
        resource->setUserCanEdit(data.userCanEdit);
        resource->setLocked(data.locked);
        resource->setBackgroundImageFilename(data.backgroundImageFilename);
        resource->setBackgroundSize(QSize(data.backgroundWidth, data.backgroundHeight));
        resource->setBackgroundOpacity(data.backgroundOpacity);
        QnLayoutItemDataList outItems;
        for (int i = 0; i < data.items.size(); ++i) {
            outItems << QnLayoutItemData();
            fromApiToResource(data.items[i], outItems.last());
        }
        resource->setItems(outItems);
    }

    void fromResourceToApi(QnLayoutResourcePtr resource, ApiLayoutData& data)
    {
        fromResourceToApi(resource, (ApiResourceData &)data);
        data.cellAspectRatio = resource->cellAspectRatio();
        data.cellSpacingWidth = resource->cellSpacing().width();
        data.cellSpacingHeight = resource->cellSpacing().height();
        data.userCanEdit = resource->userCanEdit();
        data.locked = resource->locked();
        data.backgroundImageFilename = resource->backgroundImageFilename();
        data.backgroundWidth = resource->backgroundSize().width();
        data.backgroundHeight = resource->backgroundSize().height();
        data.backgroundOpacity = resource->backgroundOpacity();
        const QnLayoutItemDataMap& layoutItems = resource->getItems();
        data.items.clear();
        data.items.reserve( layoutItems.size() );
        for( const QnLayoutItemData& item: layoutItems )
        {
            data.items.push_back( ApiLayoutItemData() );
            fromResourceToApi(item, data.items.back());
        }
    }


    template <class T>
    void ApiLayoutList::toResourceList(QList<T>& outData) const
    {
        outData.reserve(outData.size() + data.size());
        for(int i = 0; i < data.size(); ++i)
        {
            QnLayoutResourcePtr layout(new QnLayoutResource());
            fromApiToResource(data[i], layout);
            outData << layout;
        }
    }
    template void ApiLayoutList::toResourceList<QnResourcePtr>(QList<QnResourcePtr>& outData) const;
    template void ApiLayoutList::toResourceList<QnLayoutResourcePtr>(QList<QnLayoutResourcePtr>& outData) const;

    void ApiLayoutList::loadFromQuery(QSqlQuery& query)
    {
        QN_QUERY_TO_DATA_OBJECT(query, ApiLayoutData, data, ApiLayoutFields ApiResourceFields)
    }

}
