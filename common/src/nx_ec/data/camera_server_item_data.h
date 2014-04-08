#ifndef __CAMERA_SERVER_ITEM_DATA_H__
#define __CAMERA_SERVER_ITEM_DATA_H__

#include "api_data.h"
#include "ec2_resource_data.h"
#include "core/resource/camera_history.h"

namespace ec2
{
    struct ApiCameraServerItemData: public ApiData
    {
        ApiCameraServerItemData(): timestamp(0) {}

        QString  physicalId;
        QString  serverGuid;
        qint64   timestamp;

        QN_DECLARE_STRUCT_SQL_BINDER();
        void fromResource(const QnCameraHistoryItem& item);
        void toResource(QnCameraHistoryItem* const item);
    };

    #define ApiCameraServerItemFields (physicalId) (serverGuid) (timestamp)
    QN_DEFINE_STRUCT_SERIALIZATORS_BINDERS (ApiCameraServerItemData, ApiCameraServerItemFields)

    struct ApiCameraServerItemDataList: public ApiData
    {
        std::vector<ApiCameraServerItemData> data;

        void loadFromQuery(QSqlQuery& query);
        void toResourceList(QnCameraHistoryList& outData) const;
    };

    QN_DEFINE_STRUCT_SERIALIZATORS (ApiCameraServerItemDataList, (data) )
}

#endif // __CAMERA_SERVER_ITEM_DATA_H__
