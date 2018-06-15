#pragma once

#include "id_data.h"
#include "resource_data.h"

#include <vector>
#include <map>

#include <QtCore/QString>

namespace nx {
namespace vms {
namespace api {

struct VideowallItemData: Data
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
#define VideowallItemData_Fields \
    (guid)(pcGuid)(layoutGuid)(name)(snapLeft)(snapTop)(snapRight)(snapBottom)

struct VideowallScreenData: Data
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
#define VideowallScreenData_Fields \
    (pcGuid)(pcIndex)(desktopLeft)(desktopTop)(desktopWidth)(desktopHeight) \
    (layoutLeft)(layoutTop)(layoutWidth)(layoutHeight)


struct VideowallMatrixItemData: Data
{
    QnUuid itemGuid;
    QnUuid layoutGuid;
};
#define VideowallMatrixItemData_Fields (itemGuid)(layoutGuid)


struct VideowallMatrixData: IdData
{
    QString name;
    std::vector<VideowallMatrixItemData> items;
};
#define VideowallMatrixData_Fields IdData_Fields (name)(items)


struct VideowallData: ResourceData
{
    bool autorun = false;

    std::vector<VideowallItemData> items;
    std::vector<VideowallScreenData> screens;
    std::vector<VideowallMatrixData> matrices;
};
#define VideowallData_Fields ResourceData_Fields (autorun)(items)(screens)(matrices)


struct VideowallControlMessageData: Data
{
    int operation = 0;
    QnUuid videowallGuid;
    QnUuid instanceGuid;
    std::map<QString, QString> params;
};
#define VideowallControlMessageData_Fields (operation)(videowallGuid)(instanceGuid)(params)

} // namespace api
} // namespace vms
} // namespace nx

Q_DECLARE_METATYPE(nx::vms::api::VideowallData)
Q_DECLARE_METATYPE(nx::vms::api::VideowallControlMessageData)
