#ifndef QN_VIDEOWALL_DATA_I_H
#define QN_VIDEOWALL_DATA_I_H

#include "ec2_resource_data_i.h"

namespace ec2 {

    struct ApiVideowallItemData {
        QnId guid;
        QnId pc_guid;
        QnId layout_guid;
        QString name;
        int x;
        int y;
        int w;
        int h;

        QN_DECLARE_STRUCT_SQL_BINDER();
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

        QN_DECLARE_STRUCT_SQL_BINDER();
    };

#define ApiVideowallScreenData_Fields (pc_guid)(pc_index)(desktop_x)(desktop_y)(desktop_w)(desktop_h)(layout_x)(layout_y)(layout_w)(layout_h)


    struct ApiVideowallData: ApiResourceData
    {
        ApiVideowallData(): autorun(false) {}

        bool autorun;

        std::vector<ApiVideowallItemData> items;
        std::vector<ApiVideowallScreenData> screens;

        QN_DECLARE_STRUCT_SQL_BINDER();
    };

#define ApiVideowallData_Fields (autorun)(items)(screens)


    struct ApiVideowallControlMessageData {
        int operation;
        QnId videowall_guid;
        QnId instance_guid;
        std::map<QString, QString> params;
    };

#define ApiVideowallControlMessageData_Fields (operation)(videowall_guid)(instance_guid)(params)

} // namespace ec2

#endif // QN_VIDEOWALL_DATA_I_H
