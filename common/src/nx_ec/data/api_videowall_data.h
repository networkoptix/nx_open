#ifndef QN_API_VIDEOWALL_DATA_H
#define QN_API_VIDEOWALL_DATA_H

#include "api_globals.h"
#include "api_data.h"
#include "api_resource_data.h"

namespace ec2
{
    struct ApiVideowallItemData: ApiData 
    {
        ApiVideowallItemData(): ApiData(), snapLeft(0), snapTop(0), snapRight(0), snapBottom(0) {}

        QnUuid guid;
        QnUuid pcGuid;
        QnUuid layoutGuid;
        QString name;
        int snapLeft;
        int snapTop;
        int snapRight;
        int snapBottom;
    };
#define ApiVideowallItemData_Fields (guid)(pcGuid)(layoutGuid)(name)(snapLeft)(snapTop)(snapRight)(snapBottom)


    struct ApiVideowallScreenData: ApiData 
    {
        ApiVideowallScreenData(): ApiData(), pcIndex(0), desktopLeft(0), desktopTop(0), desktopWidth(0), desktopHeight(0), layoutLeft(0), layoutTop(0), layoutWidth(0), layoutHeight(0) {}

        QnUuid pcGuid;
        int pcIndex;
        int desktopLeft;
        int desktopTop;
        int desktopWidth;
        int desktopHeight;
        int layoutLeft;
        int layoutTop;
        int layoutWidth;
        int layoutHeight;
    };
#define ApiVideowallScreenData_Fields (pcGuid)(pcIndex)(desktopLeft)(desktopTop)(desktopWidth)(desktopHeight)(layoutLeft)(layoutTop)(layoutWidth)(layoutHeight)


    struct ApiVideowallMatrixItemData: ApiData {
        QnUuid itemGuid;
        QnUuid layoutGuid;
    };
#define ApiVideowallMatrixItemData_Fields (itemGuid)(layoutGuid)


    struct ApiVideowallMatrixData: ApiIdData {
        QString name;
        std::vector<ApiVideowallMatrixItemData> items;
    };
#define ApiVideowallMatrixData_Fields ApiIdData_Fields (name)(items)


    struct ApiVideowallData: ApiResourceData
    {
        ApiVideowallData(): autorun(false) {}

        bool autorun;

        std::vector<ApiVideowallItemData> items;
        std::vector<ApiVideowallScreenData> screens;
        std::vector<ApiVideowallMatrixData> matrices;
    };
#define ApiVideowallData_Fields ApiResourceData_Fields (autorun)(items)(screens)(matrices)


    struct ApiVideowallControlMessageData: ApiData 
    {
        ApiVideowallControlMessageData(): operation(0) {}

        int operation;
        QnUuid videowallGuid;
        QnUuid instanceGuid;
        std::map<QString, QString> params;
    };
#define ApiVideowallControlMessageData_Fields (operation)(videowallGuid)(instanceGuid)(params)

    struct ApiVideowallItemWithRefData: public ApiVideowallItemData {
        QnUuid videowallGuid;
    };
#define ApiVideowallItemWithRefData_Fields ApiVideowallItemData_Fields (videowallGuid)


    struct ApiVideowallScreenWithRefData: public ApiVideowallScreenData {
        QnUuid videowallGuid;
    };
#define ApiVideowallScreenWithRefData_Fields ApiVideowallScreenData_Fields (videowallGuid)


    struct ApiVideowallMatrixItemWithRefData: public ApiVideowallMatrixItemData {
        QnUuid matrixGuid;
    };
#define ApiVideowallMatrixItemWithRefData_Fields ApiVideowallMatrixItemData_Fields (matrixGuid)


    struct ApiVideowallMatrixWithRefData: public ApiVideowallMatrixData {
        QnUuid videowallGuid;
    };
#define ApiVideowallMatrixWithRefData_Fields ApiVideowallMatrixData_Fields (videowallGuid)

} // namespace ec2

#endif // QN_API_VIDEOWALL_DATA_H
