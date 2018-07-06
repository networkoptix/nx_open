#pragma once

#include "id_data.h"
#include "resource_data.h"

#include <vector>
#include <map>

#include <QtCore/QString>

namespace nx {
namespace vms {
namespace api {

struct NX_VMS_API VideowallItemData: Data
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

struct NX_VMS_API VideowallScreenData: Data
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

struct NX_VMS_API VideowallMatrixItemData: Data
{
    QnUuid itemGuid;
    QnUuid layoutGuid;
};
#define VideowallMatrixItemData_Fields (itemGuid)(layoutGuid)

struct NX_VMS_API VideowallMatrixData: IdData
{
    QString name;
    VideowallMatrixItemDataList items;
};
#define VideowallMatrixData_Fields IdData_Fields (name)(items)

struct NX_VMS_API VideowallData: ResourceData
{
    VideowallData(): ResourceData(kResourceTypeId) {}

    static const QString kResourceTypeName;
    static const QnUuid kResourceTypeId;

    bool autorun = false;
    bool timeline = false;

    VideowallItemDataList items;
    VideowallScreenDataList screens;
    VideowallMatrixDataList matrices;
};
#define VideowallData_Fields ResourceData_Fields (autorun)(items)(screens)(matrices)(timeline)

struct NX_VMS_API VideowallControlMessageData: Data
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
Q_DECLARE_METATYPE(nx::vms::api::VideowallDataList)
Q_DECLARE_METATYPE(nx::vms::api::VideowallItemData)
Q_DECLARE_METATYPE(nx::vms::api::VideowallItemDataList)
Q_DECLARE_METATYPE(nx::vms::api::VideowallScreenData)
Q_DECLARE_METATYPE(nx::vms::api::VideowallScreenDataList)
Q_DECLARE_METATYPE(nx::vms::api::VideowallMatrixData)
Q_DECLARE_METATYPE(nx::vms::api::VideowallMatrixDataList)
Q_DECLARE_METATYPE(nx::vms::api::VideowallMatrixItemData)
Q_DECLARE_METATYPE(nx::vms::api::VideowallMatrixItemDataList)
Q_DECLARE_METATYPE(nx::vms::api::VideowallControlMessageData)
