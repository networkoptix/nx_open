#pragma once

#include "api_globals.h"
#include "api_data.h"
#include "api_resource_data.h"

namespace ec2 {

struct ApiVideowallItemData: nx::vms::api::Data
{
    QnUuid guid;
    QnUuid pcGuid;
    QnUuid layoutGuid;
    QString name;
    int snapLeft = 0;
    int snapTop = 0;
    int snapRight = 0;
    int snapBottom = 0;
};
#define ApiVideowallItemData_Fields \
    (guid)(pcGuid)(layoutGuid)(name)(snapLeft)(snapTop)(snapRight)(snapBottom)

struct ApiVideowallScreenData: nx::vms::api::Data
{
    QnUuid pcGuid;
    int pcIndex = 0;
    int desktopLeft = 0;
    int desktopTop = 0;
    int desktopWidth = 0;
    int desktopHeight = 0;
    int layoutLeft = 0;
    int layoutTop = 0;
    int layoutWidth = 0;
    int layoutHeight = 0;
};
#define ApiVideowallScreenData_Fields \
    (pcGuid)(pcIndex)(desktopLeft)(desktopTop)(desktopWidth)(desktopHeight) \
    (layoutLeft)(layoutTop)(layoutWidth)(layoutHeight)


struct ApiVideowallMatrixItemData: nx::vms::api::Data
{
    QnUuid itemGuid;
    QnUuid layoutGuid;
};
#define ApiVideowallMatrixItemData_Fields (itemGuid)(layoutGuid)


struct ApiVideowallMatrixData: nx::vms::api::IdData
{
    QString name;
    std::vector<ApiVideowallMatrixItemData> items;
};
#define ApiVideowallMatrixData_Fields IdData_Fields (name)(items)


struct ApiVideowallData: nx::vms::api::ResourceData
{
    bool autorun = false;

    std::vector<ApiVideowallItemData> items;
    std::vector<ApiVideowallScreenData> screens;
    std::vector<ApiVideowallMatrixData> matrices;
};
#define ApiVideowallData_Fields ResourceData_Fields (autorun)(items)(screens)(matrices)


struct ApiVideowallControlMessageData: nx::vms::api::Data
{
    int operation = 0;
    QnUuid videowallGuid;
    QnUuid instanceGuid;
    std::map<QString, QString> params;
};
#define ApiVideowallControlMessageData_Fields (operation)(videowallGuid)(instanceGuid)(params)

struct ApiVideowallItemWithRefData: ApiVideowallItemData
{
    QnUuid videowallGuid;
};
#define ApiVideowallItemWithRefData_Fields ApiVideowallItemData_Fields (videowallGuid)

struct ApiVideowallScreenWithRefData: ApiVideowallScreenData
{
    QnUuid videowallGuid;
};
#define ApiVideowallScreenWithRefData_Fields ApiVideowallScreenData_Fields (videowallGuid)


struct ApiVideowallMatrixItemWithRefData: ApiVideowallMatrixItemData
{
    QnUuid matrixGuid;
};
#define ApiVideowallMatrixItemWithRefData_Fields ApiVideowallMatrixItemData_Fields (matrixGuid)


struct ApiVideowallMatrixWithRefData: ApiVideowallMatrixData
{
    QnUuid videowallGuid;
};
#define ApiVideowallMatrixWithRefData_Fields ApiVideowallMatrixData_Fields (videowallGuid)

} // namespace ec2
