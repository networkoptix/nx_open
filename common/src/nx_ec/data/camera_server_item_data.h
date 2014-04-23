#ifndef __CAMERA_SERVER_ITEM_DATA_H__
#define __CAMERA_SERVER_ITEM_DATA_H__

#include "api_data.h"
#include "ec2_resource_data.h"
#include "core/resource/camera_history.h"
#include "camera_server_item_data_i.h"

namespace ec2
{
    struct ApiCameraServerItem: ApiCameraServerItemData
    {
        QN_DECLARE_STRUCT_SQL_BINDER();
        void fromResource(const QnCameraHistoryItem& item);
        void toResource(QnCameraHistoryItem* const item);
    };

    QN_DEFINE_STRUCT_SQL_BINDER(ApiCameraServerItem, ApiCameraServerItemFields);


    struct ApiCameraServerItemList: ApiCameraServerItemListData
    {
        void loadFromQuery(QSqlQuery& query);
        void toResourceList(QnCameraHistoryList& outData) const;
    };
}

#endif // __CAMERA_SERVER_ITEM_DATA_H__