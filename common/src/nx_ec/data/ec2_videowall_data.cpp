#include "ec2_videowall_data.h"
#include "core/resource/videowall_resource.h"

namespace ec2
{

    void ApiVideowallItem::toItem(QnVideoWallItem& item) const {
        item.uuid       = guid;
        item.layout     = layout_guid;
        item.pcUuid     = pc_guid;
        item.name       = name;
        item.geometry   = QRect(x, y, w, h);
    }

    void ApiVideowallItem::fromItem(const QnVideoWallItem& item) {
        guid        = item.uuid;
        layout_guid = item.layout;
        pc_guid     = item.pcUuid;
        name        = item.name;
        x           = item.geometry.x();
        y           = item.geometry.y();
        w           = item.geometry.width();
        h           = item.geometry.height();
    }

    void ApiVideowallScreen::toScreen(QnVideoWallPcData::PcScreen& screen) const {
        screen.index            = pc_index;
        screen.desktopGeometry  = QRect(desktop_x, desktop_y, desktop_w, desktop_h);
        screen.layoutGeometry   = QRect(layout_x, layout_y, layout_w, layout_h);
    }

    void ApiVideowallScreen::fromScreen(const QnVideoWallPcData::PcScreen& screen) {
        pc_index    = screen.index;
        desktop_x   = screen.desktopGeometry.x();
        desktop_y   = screen.desktopGeometry.y();
        desktop_w   = screen.desktopGeometry.width();
        desktop_h   = screen.desktopGeometry.height();
        layout_x    = screen.layoutGeometry.x();
        layout_y    = screen.layoutGeometry.y();
        layout_w    = screen.layoutGeometry.width();
        layout_h    = screen.layoutGeometry.height();
    }

    void ApiVideowall::toResource(QnVideoWallResourcePtr resource) const {
        ApiResource::toResource(resource);
        resource->setAutorun(autorun);
        QnVideoWallItemList outItems;
        for (const ApiVideowallItem &item : items) {
            outItems << QnVideoWallItem();
            item.toItem(outItems.last());
        }
        resource->setItems(outItems);

        QnVideoWallPcDataMap pcs;
        for (const ApiVideowallScreen &screen : screens) {
            QnVideoWallPcData::PcScreen outScreen;
            screen.toScreen(outScreen);
            pcs[screen.pc_guid].screens << outScreen;
        }
        resource->setPcs(pcs);

    }
    
    void ApiVideowall::fromResource(const QnVideoWallResourcePtr &resource) {
        ApiResource::fromResource(resource);
        autorun = resource->isAutorun();

        const QnVideoWallItemMap& resourceItems = resource->getItems();
        items.clear();
        items.reserve(resourceItems.size());
        for (const QnVideoWallItem &item: resourceItems) {
            ApiVideowallItem itemData;
            itemData.fromItem(item);
            items.push_back(itemData);
        }

        screens.clear();
        for (const QnVideoWallPcData &pc: resource->getPcs()) {
            for (const QnVideoWallPcData::PcScreen &screen: pc.screens) {
                ApiVideowallScreen screenData;
                screenData.fromScreen(screen);
                screenData.pc_guid = pc.uuid;
                screens.push_back(screenData);
            }
        }
    }

    template <class T>
    void ApiVideowallList::toResourceList(QList<T>& outData) const {
        outData.reserve(outData.size() + data.size());
        for(int i = 0; i < data.size(); ++i) 
        {
            QnVideoWallResourcePtr videowall(new QnVideoWallResource());
            data[i].toResource(videowall);
            outData << videowall;
        }
    }
    template void ApiVideowallList::toResourceList<QnResourcePtr>(QList<QnResourcePtr>& outData) const;
    template void ApiVideowallList::toResourceList<QnVideoWallResourcePtr>(QList<QnVideoWallResourcePtr>& outData) const;

    void ApiVideowallList::loadFromQuery(QSqlQuery& query) {
        QN_QUERY_TO_DATA_OBJECT(query, ApiVideowall, data, ApiVideowallDataFields ApiResourceFields)
    }

}
