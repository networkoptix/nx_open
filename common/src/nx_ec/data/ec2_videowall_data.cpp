#include "ec2_videowall_data.h"
#include "core/resource/videowall_resource.h"

namespace ec2
{

    void ApiVideowallItemData::toItem(QnVideoWallItem& item) const {
        item.uuid       = guid;
        item.layout     = layout_guid;
        item.pcUuid     = pc_guid;
        item.name       = name;
        item.geometry   = QRect(x, y, w, h);
    }

    void ApiVideowallItemData::fromItem(const QnVideoWallItem& item) {
        guid        = item.uuid;
        layout_guid = item.layout;
        pc_guid     = item.pcUuid;
        name        = item.name;
        x           = item.geometry.x();
        y           = item.geometry.y();
        w           = item.geometry.width();
        h           = item.geometry.height();
    }

    void ApiVideowallData::toResource(QnVideoWallResourcePtr resource) const {
        ApiResourceData::toResource(resource);
        resource->setAutorun(autorun);
        QnVideoWallItemList outItems;
        for (int i = 0; i < items.size(); ++i) {
            outItems << QnVideoWallItem();
            items[i].toItem(outItems.last());
        }
        resource->setItems(outItems);
    }
    
    void ApiVideowallData::fromResource(const QnVideoWallResourcePtr &resource) {
        ApiResourceData::fromResource(resource);
        autorun = resource->isAutorun();
        const QnVideoWallItemMap& resourceItems = resource->getItems();
        items.clear();
        items.reserve( resourceItems.size() );
        for( const QnVideoWallItem& item: resourceItems )
        {
            items.push_back( ApiVideowallItemData() );
            items.back().fromItem( item );
        }
    }

    template <class T>
    void ApiVideowallDataList::toResourceList(QList<T>& outData) const {
        outData.reserve(outData.size() + data.size());
        for(int i = 0; i < data.size(); ++i) 
        {
            QnVideoWallResourcePtr videowall(new QnVideoWallResource());
            data[i].toResource(videowall);
            outData << videowall;
        }
    }
    template void ApiVideowallDataList::toResourceList<QnResourcePtr>(QList<QnResourcePtr>& outData) const;
    template void ApiVideowallDataList::toResourceList<QnVideoWallResourcePtr>(QList<QnVideoWallResourcePtr>& outData) const;

    void ApiVideowallDataList::loadFromQuery(QSqlQuery& query) {
        QN_QUERY_TO_DATA_OBJECT(query, ApiVideowallData, data, ApiVideowallDataFields ApiResourceDataFields)
    }

}
