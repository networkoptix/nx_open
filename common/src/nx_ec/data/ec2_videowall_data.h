#ifndef __EC2_VIDEOWALL_DATA_H_
#define __EC2_VIDEOWALL_DATA_H_

#include <core/resource/resource_fwd.h>
#include <core/resource/videowall_item.h>
#include <core/resource/videowall_pc_data.h>
#include <core/resource/videowall_control_message.h>

#include <nx_ec/data/ec2_resource_data.h>

namespace ec2
{
    struct ApiVideowallItem;
    struct ApiVideowallScreen;
    struct ApiVideowall;

    #include "ec2_videowall_data_i.h"

    struct ApiVideowallItem : ApiVideowallItemData {
        void toItem(QnVideoWallItem& item) const;
        void fromItem(const QnVideoWallItem& item);
        QN_DECLARE_STRUCT_SQL_BINDER();
    };

    QN_DEFINE_STRUCT_SQL_BINDER (ApiVideowallItem, ApiVideowallItemDataFields)

    struct ApiVideowallItemDataWithRef: public ApiVideowallItem {
        QnId videowall_guid;
    };

    struct ApiVideowallScreen : ApiVideowallScreenData {
        void toScreen(QnVideoWallPcData::PcScreen& screen) const;
        void fromScreen(const QnVideoWallPcData::PcScreen& screen);
        QN_DECLARE_STRUCT_SQL_BINDER();
    };

    QN_DEFINE_STRUCT_SQL_BINDER (ApiVideowallScreen, ApiVideowallScreenDataFields)

    struct ApiVideowallScreenDataWithRef: public ApiVideowallScreen {
        QnId videowall_guid;
    };

    struct ApiVideowall: ApiVideowallData, ApiResource
    {
        void toResource(QnVideoWallResourcePtr resource) const;
        void fromResource(const QnVideoWallResourcePtr &resource);
        QN_DECLARE_STRUCT_SQL_BINDER();
    };

    QN_DEFINE_STRUCT_SQL_BINDER(ApiVideowall, ApiVideowallDataFields)

    struct ApiVideowallList: ApiVideowallListData
    {
        void loadFromQuery(QSqlQuery& query);
        template <class T> void toResourceList(QList<T>& outData) const;
    };

    struct ApiVideowallControlMessage: ApiVideowallControlMessageData {
        void toMessage(QnVideoWallControlMessage &message) const;
        void fromMessage(const QnVideoWallControlMessage &message);
    };
}

#endif // __EC2_VIDEOWALL_DATA_H_
