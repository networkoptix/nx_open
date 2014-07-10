#ifndef QN_API_VIDEOWALL_DATA_H
#define QN_API_VIDEOWALL_DATA_H

#include "api_globals.h"
#include "api_data.h"
#include "api_resource_data.h"

namespace ec2
{
    struct ApiVideowallItemData: ApiData {
        QnId guid;
        QnId pcGuid;
        QnId layoutGuid;
        QString name;
        int snapLeft;
        int snapTop;
        int snapRight;
        int snapBottom;
    };
#define ApiVideowallItemData_Fields (guid)(pcGuid)(layoutGuid)(name)(snapLeft)(snapTop)(snapRight)(snapBottom)


    struct ApiVideowallScreenData: ApiData {
        QnId pcGuid;
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
        QnId itemGuid;
        QnId layoutGuid;
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


    struct ApiVideowallControlMessageData: ApiData {
        int operation;
        QnId videowallGuid;
        QnId instanceGuid;
        std::map<QString, QString> params;
    };
#define ApiVideowallControlMessageData_Fields (operation)(videowallGuid)(instanceGuid)(params)

    struct ApiVideowallItemWithRefData: public ApiVideowallItemData {
        QnId videowallGuid;
    };
#define ApiVideowallItemWithRefData_Fields ApiVideowallItemData_Fields (videowallGuid)


    struct ApiVideowallScreenWithRefData: public ApiVideowallScreenData {
        QnId videowallGuid;
    };
#define ApiVideowallScreenWithRefData_Fields ApiVideowallScreenData_Fields (videowallGuid)


    struct ApiVideowallMatrixItemWithRefData: public ApiVideowallMatrixItemData {
        QnId matrixGuid;
    };
#define ApiVideowallMatrixItemWithRefData_Fields ApiVideowallMatrixItemData_Fields (matrixGuid)


    struct ApiVideowallMatrixWithRefData: public ApiVideowallMatrixData {
        QnId videowallGuid;
    };
#define ApiVideowallMatrixWithRefData_Fields ApiVideowallMatrixData_Fields (videowallGuid)

} // namespace ec2

#endif // QN_API_VIDEOWALL_DATA_H
