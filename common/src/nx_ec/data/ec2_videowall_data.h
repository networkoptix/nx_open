#ifndef __EC2_VIDEOWALL_DATA_H_
#define __EC2_VIDEOWALL_DATA_H_

#include <core/resource/resource_fwd.h>
#include <core/resource/videowall_item.h>
#include <core/resource/videowall_pc_data.h>
#include <core/resource/videowall_control_message.h>

#include <nx_ec/data/ec2_resource_data.h>
#include "ec2_videowall_data_i.h"

namespace ec2
{

    void fromApiToItem(const ApiVideowallItemData &data, QnVideoWallItem& item);
    void fromItemToApi(const QnVideoWallItem& item, ApiVideowallItemData &data);

    QN_DEFINE_STRUCT_SQL_BINDER (ApiVideowallItemData, ApiVideowallItemDataFields)

    struct ApiVideowallItemDataWithRef: public ApiVideowallItemData {
        QnId videowall_guid;
    };

    void fromApiToScreen(const ApiVideowallScreenData &data, QnVideoWallPcData::PcScreen& screen);
    void fromScreenToApi(const QnVideoWallPcData::PcScreen& screen, ApiVideowallScreenData& data);

    QN_DEFINE_STRUCT_SQL_BINDER (ApiVideowallScreenData, ApiVideowallScreenDataFields)

    struct ApiVideowallScreenDataWithRef: public ApiVideowallScreenData {
        QnId videowall_guid;
    };

    void fromApiToResource(const ApiVideowallData &data, QnVideoWallResourcePtr resource);
    void fromResourceToApi(const QnVideoWallResourcePtr &resource, ApiVideowallData &data);

    QN_DEFINE_STRUCT_SQL_BINDER(ApiVideowallData, ApiVideowallDataFields)

    struct ApiVideowallList: ApiVideowallDataListData
    {
        void loadFromQuery(QSqlQuery& query);
        template <class T> void toResourceList(QList<T>& outData) const;
    };

    void fromApiToMessage(const ApiVideowallControlMessageData &data, QnVideoWallControlMessage &message);
    void fromMessageToApi(const QnVideoWallControlMessage &message, ApiVideowallControlMessageData &data);
}

#endif // __EC2_VIDEOWALL_DATA_H_
