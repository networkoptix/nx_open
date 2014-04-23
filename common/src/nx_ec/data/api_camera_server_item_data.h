#ifndef __CAMERA_SERVER_ITEM_DATA_H__
#define __CAMERA_SERVER_ITEM_DATA_H__

#include "api_globals.h"
#include "api_data.h"

namespace ec2
{
    struct ApiCameraServerItemData : ApiData {
        ApiCameraServerItemData(): timestamp(0) {}

        QString  physicalId;
        QString  serverGuid;
        qint64   timestamp;
    };
#define ApiCameraServerItemData_Fields (physicalId)(serverGuid)(timestamp)

/*
    struct ApiCameraServerItem: ApiCameraServerItemData
    {
        //QN_DECLARE_STRUCT_SQL_BINDER();
        void fromResource(const QnCameraHistoryItem& item);
        void toResource(QnCameraHistoryItem* const item);
    };
*/

    //QN_DEFINE_STRUCT_SQL_BINDER(ApiCameraServerItem, ApiCameraServerItemFields);


    /*struct ApiCameraServerItemList: ApiCameraServerItemListData
    {
        void loadFromQuery(QSqlQuery& query);
        void toResourceList(QnCameraHistoryList& outData) const;
    };*/
}

#endif // __CAMERA_SERVER_ITEM_DATA_H__
