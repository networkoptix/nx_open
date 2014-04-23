#include "ec2_videowall_data.h"

#include <core/resource/videowall_resource.h>

//QN_DEFINE_STRUCT_SERIALIZATORS (ApiVideowallItemData, ApiVideowallItemDataFields)
QN_FUSION_DECLARE_FUNCTIONS(ApiVideowallItemData, (binary))

    //QN_DEFINE_STRUCT_SERIALIZATORS (ApiVideowallScreenData, ApiVideowallScreenDataFields)
    QN_FUSION_DECLARE_FUNCTIONS(ApiVideowallScreenData, (binary))

    //QN_DEFINE_DERIVED_STRUCT_SERIALIZATORS(ApiVideowallData, ApiResourceData, ApiVideowallDataFields);
    QN_FUSION_DECLARE_FUNCTIONS(ApiVideowallData, (binary))

    QN_DEFINE_API_OBJECT_LIST_DATA(ApiVideowallData)

    //QN_DEFINE_STRUCT_SERIALIZATORS (ApiVideowallControlMessageData, ApiVideowallControlMessageDataFields)
    QN_FUSION_DECLARE_FUNCTIONS(ApiVideowallControlMessageData, (binary))


namespace ec2
{

    void fromApiToItem(const ApiVideowallItemData &data, QnVideoWallItem& item) {
        item.uuid       = data.guid;
        item.layout     = data.layout_guid;
        item.pcUuid     = data.pc_guid;
        item.name       = data.name;
        item.geometry   = QRect(data.x, data.y, data.w, data.h);
    }

    void fromItemToApi(const QnVideoWallItem& item, ApiVideowallItemData &data) {
        data.guid        = item.uuid;
        data.layout_guid = item.layout;
        data.pc_guid     = item.pcUuid;
        data.name        = item.name;
        data.x           = item.geometry.x();
        data.y           = item.geometry.y();
        data.w           = item.geometry.width();
        data.h           = item.geometry.height();
    }

    void fromApiToScreen(const ApiVideowallScreenData &data, QnVideoWallPcData::PcScreen& screen) {
        screen.index            = data.pc_index;
        screen.desktopGeometry  = QRect(data.desktop_x, data.desktop_y, data.desktop_w, data.desktop_h);
        screen.layoutGeometry   = QRect(data.layout_x, data.layout_y, data.layout_w, data.layout_h);
    }

    void fromScreenToApi(const QnVideoWallPcData::PcScreen& screen, ApiVideowallScreenData &data) {
        data.pc_index    = screen.index;
        data.desktop_x   = screen.desktopGeometry.x();
        data.desktop_y   = screen.desktopGeometry.y();
        data.desktop_w   = screen.desktopGeometry.width();
        data.desktop_h   = screen.desktopGeometry.height();
        data.layout_x    = screen.layoutGeometry.x();
        data.layout_y    = screen.layoutGeometry.y();
        data.layout_w    = screen.layoutGeometry.width();
        data.layout_h    = screen.layoutGeometry.height();
    }

    void fromApiToResource(const ApiVideowallData &data, QnVideoWallResourcePtr resource) {
        fromApiToResource((const ApiResourceData &)data, resource);
        resource->setAutorun(data.autorun);
        QnVideoWallItemList outItems;
        for (const ApiVideowallItemData &item : data.items) {
            outItems << QnVideoWallItem();
            fromApiToItem(item, outItems.last());
        }
        resource->setItems(outItems);

        QnVideoWallPcDataMap pcs;
        for (const ApiVideowallScreenData &screen : data.screens) {
            QnVideoWallPcData::PcScreen outScreen;
            fromApiToScreen(screen, outScreen);
            QnVideoWallPcData& outPc = pcs[screen.pc_guid];
            outPc.uuid = screen.pc_guid;
            outPc.screens << outScreen;
        }
        resource->setPcs(pcs);

    }
    
    void fromResourceToApi(const QnVideoWallResourcePtr &resource, ApiVideowallData &data) {
        fromResourceToApi(resource, (ApiResourceData &)data);
        data.autorun = resource->isAutorun();

        const QnVideoWallItemMap& resourceItems = resource->getItems();
        data.items.clear();
        data.items.reserve(resourceItems.size());
        for (const QnVideoWallItem &item: resourceItems) {
            ApiVideowallItemData itemData;
            fromItemToApi(item, itemData);
            data.items.push_back(itemData);
        }

        data.screens.clear();
        for (const QnVideoWallPcData &pc: resource->getPcs()) {
            for (const QnVideoWallPcData::PcScreen &screen: pc.screens) {
                ApiVideowallScreenData screenData;
                fromScreenToApi(screen, screenData);
                screenData.pc_guid = pc.uuid;
                data.screens.push_back(screenData);
            }
        }
    }

    template <class T>
    void ApiVideowallList::toResourceList(QList<T>& outData) const {
        outData.reserve(outData.size() + data.size());
        for(int i = 0; i < data.size(); ++i) 
        {
            QnVideoWallResourcePtr videowall(new QnVideoWallResource());
            fromApiToResource(data[i], videowall);
            outData << videowall;
        }
    }
    template void ApiVideowallList::toResourceList<QnResourcePtr>(QList<QnResourcePtr>& outData) const;
    template void ApiVideowallList::toResourceList<QnVideoWallResourcePtr>(QList<QnVideoWallResourcePtr>& outData) const;

    void ApiVideowallList::loadFromQuery(QSqlQuery& query) {
        QN_QUERY_TO_DATA_OBJECT(query, ApiVideowallData, data, ApiVideowallDataFields ApiResourceFields)
    }


    void fromApiToMessage(const ApiVideowallControlMessageData &data, QnVideoWallControlMessage &message) {
        message.operation = static_cast<QnVideoWallControlMessage::QnVideoWallControlOperation>(data.operation);
        message.videoWallGuid = data.videowall_guid;
        message.instanceGuid = data.instance_guid;
        message.params.clear();
        for (std::pair<QString, QString> pair : data.params)
            message.params[pair.first] = pair.second;
    }

    void fromMessageToApi(const QnVideoWallControlMessage &message, ApiVideowallControlMessageData &data) {
        data.operation = static_cast<int>(message.operation);
        data.videowall_guid = message.videoWallGuid;
        data.instance_guid = message.instanceGuid;
        data.params.clear();
        auto iter = message.params.constBegin();
        while (iter != message.params.constEnd()) {
            data.params.insert(std::pair<QString, QString>(iter.key(), iter.value()));
            ++iter;
        }
    }
}
