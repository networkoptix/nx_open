#ifndef __EC2_VIDEOWALL_DATA_H_
#define __EC2_VIDEOWALL_DATA_H_

#include "api_globals.h"
#include "api_resource_data.h"

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

        //QN_DECLARE_STRUCT_SQL_BINDER();
    };
#define ApiVideowallItemData_Fields (guid)(pc_guid)(layout_guid)(name)(x)(y)(w)(h)


    struct ApiVideowallScreenData {
        QnId pc_guid;
        int pc_index;
        int desktop_x;
        int desktop_y;
        int desktop_w;
        int desktop_h;
        int layout_x;
        int layout_y;
        int layout_w;
        int layout_h;

        //QN_DECLARE_STRUCT_SQL_BINDER();
    };
#define ApiVideowallScreenData_Fields (pc_guid)(pc_index)(desktop_x)(desktop_y)(desktop_w)(desktop_h)(layout_x)(layout_y)(layout_w)(layout_h)


    struct ApiVideowallData: ApiResourceData
    {
        ApiVideowallData(): autorun(false) {}

        bool autorun;

        std::vector<ApiVideowallItemData> items;
        std::vector<ApiVideowallScreenData> screens;

        //QN_DECLARE_STRUCT_SQL_BINDER();
    };
#define ApiVideowallData_Fields (autorun)(items)(screens)


    struct ApiVideowallControlMessageData {
        int operation;
        QnId videowall_guid;
        QnId instance_guid;
        std::map<QString, QString> params;
    };
#define ApiVideowallControlMessageData_Fields (operation)(videowall_guid)(instance_guid)(params)

    /*void fromApiToItem(const ApiVideowallItemData &data, QnVideoWallItem& item);
    void fromItemToApi(const QnVideoWallItem& item, ApiVideowallItemData &data);*/

    //QN_DEFINE_STRUCT_SQL_BINDER (ApiVideowallItemData, ApiVideowallItemDataFields)

    struct ApiVideowallItemWithRefData: public ApiVideowallItemData {
        QnId videowall_guid;
    };

    struct ApiVideowallScreenWithRefData: public ApiVideowallScreenData {
        QnId videowall_guid;
    };


    /*void fromApiToScreen(const ApiVideowallScreenData &data, QnVideoWallPcData::PcScreen& screen);
    void fromScreenToApi(const QnVideoWallPcData::PcScreen& screen, ApiVideowallScreenData& data);

    //QN_DEFINE_STRUCT_SQL_BINDER (ApiVideowallScreenData, ApiVideowallScreenDataFields)


    void fromApiToResource(const ApiVideowallData &data, QnVideoWallResourcePtr resource);
    void fromResourceToApi(const QnVideoWallResourcePtr &resource, ApiVideowallData &data);

    //QN_DEFINE_STRUCT_SQL_BINDER(ApiVideowallData, ApiVideowallDataFields)

    struct ApiVideowallList: std::vector<ApiVideowallData>
    {
        void loadFromQuery(QSqlQuery& query);
        template <class T> void toResourceList(QList<T>& outData) const;
    };

    void fromApiToMessage(const ApiVideowallControlMessageData &data, QnVideoWallControlMessage &message);
    void fromMessageToApi(const QnVideoWallControlMessage &message, ApiVideowallControlMessageData &data);
}

#endif // __EC2_VIDEOWALL_DATA_H_
