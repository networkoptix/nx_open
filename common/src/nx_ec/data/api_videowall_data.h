#ifndef QN_API_VIDEOWALL_DATA_H
#define QN_API_VIDEOWALL_DATA_H

#include "api_globals.h"
#include "api_resource_data.h"

namespace ec2
{
    struct ApiVideowallItemData {
        QnId guid;
        QnId pcGuid;
        QnId layoutGuid;
        QString name;
        int left;
        int top;
        int width;
        int height;
    };
#define ApiVideowallItemData_Fields (guid)(pcGuid)(layoutGuid)(name)(left)(top)(width)(height)

    struct ApiVideowallScreenData {
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


    struct ApiVideowallData: ApiResourceData
    {
        ApiVideowallData(): autorun(false) {}

        bool autorun;

        std::vector<ApiVideowallItemData> items;
        std::vector<ApiVideowallScreenData> screens;
    };
#define ApiVideowallData_Fields ApiResourceData_Fields (autorun)(items)(screens)


    struct ApiVideowallControlMessageData {
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

} // namespace ec2

#endif // QN_API_VIDEOWALL_DATA_H
