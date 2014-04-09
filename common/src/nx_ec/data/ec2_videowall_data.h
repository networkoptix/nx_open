#ifndef __EC2_VIDEOWALL_DATA_H_
#define __EC2_VIDEOWALL_DATA_H_

#include <core/resource/resource_fwd.h>
#include <core/resource/videowall_item.h>
#include <core/resource/videowall_pc_data.h>

#include <nx_ec/data/ec2_resource_data.h>

namespace ec2
{
    struct ApiVideowallItemData {
        QnId guid;
        QnId pc_guid;
        QnId layout_guid;
        QString name;
        int x;
        int y;
        int w;
        int h;

        void toItem(QnVideoWallItem& item) const;
        void fromItem(const QnVideoWallItem& item);
        QN_DECLARE_STRUCT_SQL_BINDER();
    };

    #define ApiVideowallItemDataFields (guid) (pc_guid) (layout_guid) (name) (x) (y) (w) (h)
    QN_DEFINE_STRUCT_SERIALIZATORS_BINDERS (ApiVideowallItemData, ApiVideowallItemDataFields)

    struct ApiVideowallItemDataWithRef: public ApiVideowallItemData {
        QnId videowall_guid;
    };

    struct ApiVideowallScreenData {
        QnId pc;
        int index;
        int desktop_x;
        int desktop_y;
        int desktop_w;
        int desktop_h;
        int layout_x;
        int layout_y;
        int layout_w;
        int layout_h;
    };

    struct ApiVideowallData: public ApiResourceData
    {
        ApiVideowallData(): autorun(false) {}
    
        bool autorun;
        std::vector<ApiVideowallItemData> items;

        void toResource(QnVideoWallResourcePtr resource) const;
        void fromResource(const QnVideoWallResourcePtr &resource);
        QN_DECLARE_STRUCT_SQL_BINDER();
    };

    #define ApiVideowallDataFields (autorun)
    QN_DEFINE_DERIVED_STRUCT_SERIALIZATORS_BINDERS(ApiVideowallData, ec2::ApiResourceData, ApiVideowallDataFields)


    struct ApiVideowallDataList: public ApiData
    {
        std::vector<ApiVideowallData> data;

        void loadFromQuery(QSqlQuery& query);
        template <class T> void toResourceList(QList<T>& outData) const;
    };

    QN_DEFINE_STRUCT_SERIALIZATORS (ApiVideowallDataList, (data) )
}

#endif // __EC2_VIDEOWALL_DATA_H_
