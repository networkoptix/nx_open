// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <map>
#include <vector>

#include <QtCore/QString>

#include <nx/reflect/instrument.h>

#include "id_data.h"
#include "resource_data.h"

namespace nx {
namespace vms {
namespace api {

struct NX_VMS_API VideowallItemData
{
    nx::Uuid guid;
    nx::Uuid pcGuid;
    nx::Uuid layoutGuid;

    /**%apidoc
     * %example Video Wall item 1
     */
    QString name;

    /**%apidoc[opt] */
    int snapLeft = 0;

    /**%apidoc[opt] */
    int snapTop = 0;

    /**%apidoc[opt] */
    int snapRight = 0;

    /**%apidoc[opt] */
    int snapBottom = 0;

    bool operator==(const VideowallItemData& other) const = default;
};
#define VideowallItemData_Fields \
    (guid)(pcGuid)(layoutGuid)(name)(snapLeft)(snapTop)(snapRight)(snapBottom)
NX_VMS_API_DECLARE_STRUCT_AND_LIST(VideowallItemData)
NX_REFLECTION_INSTRUMENT(VideowallItemData, VideowallItemData_Fields)

struct NX_VMS_API VideowallScreenData
{
    nx::Uuid pcGuid;

    /**%apidoc[opt] */
    int pcIndex = 0;

    /**%apidoc[opt] */
    int desktopLeft = 0;

    /**%apidoc[opt] */
    int desktopTop = 0;

    /**%apidoc[opt] */
    int desktopWidth = 0;

    /**%apidoc[opt] */
    int desktopHeight = 0;

    /**%apidoc[opt] */
    int layoutLeft = 0;

    /**%apidoc[opt] */
    int layoutTop = 0;

    /**%apidoc[opt] */
    int layoutWidth = 0;

    /**%apidoc[opt] */
    int layoutHeight = 0;

    bool operator==(const VideowallScreenData& other) const = default;
};
#define VideowallScreenData_Fields \
    (pcGuid)(pcIndex)(desktopLeft)(desktopTop)(desktopWidth)(desktopHeight) \
    (layoutLeft)(layoutTop)(layoutWidth)(layoutHeight)
NX_VMS_API_DECLARE_STRUCT_AND_LIST(VideowallScreenData)
NX_REFLECTION_INSTRUMENT(VideowallScreenData, VideowallScreenData_Fields)

struct NX_VMS_API VideowallMatrixItemData
{
    nx::Uuid itemGuid;
    nx::Uuid layoutGuid;

    bool operator==(const VideowallMatrixItemData& other) const = default;
};
#define VideowallMatrixItemData_Fields (itemGuid)(layoutGuid)
NX_VMS_API_DECLARE_STRUCT_AND_LIST(VideowallMatrixItemData)
NX_REFLECTION_INSTRUMENT(VideowallMatrixItemData, VideowallMatrixItemData_Fields)

struct NX_VMS_API VideowallMatrixData: IdData
{
    /**%apidoc
     * %example Video Wall matrix 1
     */
    QString name;

    VideowallMatrixItemDataList items;

    bool operator==(const VideowallMatrixData& other) const = default;
};
#define VideowallMatrixData_Fields IdData_Fields (name)(items)
NX_VMS_API_DECLARE_STRUCT_AND_LIST(VideowallMatrixData)
NX_REFLECTION_INSTRUMENT(VideowallMatrixData, VideowallMatrixData_Fields)

/**%apidoc Video wall information object.
 * %param[proprietary] typeId
 * %param[opt] parentId Id of a User who created this video wall.
 * %param[immutable] id Video wall unique id.
 * %param name Video wall name.
 *     %example Video wall
 * %param[unused] url
 */
struct NX_VMS_API VideowallData: ResourceData
{
    VideowallData(): ResourceData(kResourceTypeId) {}

    bool operator==(const VideowallData& other) const = default;

    static const QString kResourceTypeName;
    static const nx::Uuid kResourceTypeId;

    /**%apidoc[opt] */
    bool autorun = false;

    /**%apidoc[opt] */
    bool timeline = false;

    VideowallItemDataList items;
    VideowallScreenDataList screens;
    VideowallMatrixDataList matrices;
};
#define VideowallData_Fields ResourceData_Fields (autorun)(items)(screens)(matrices)(timeline)
NX_VMS_API_DECLARE_STRUCT_AND_LIST(VideowallData)
NX_REFLECTION_INSTRUMENT(VideowallData, VideowallData_Fields)

struct NX_VMS_API VideowallControlMessageData
{
    int operation = 0;
    nx::Uuid videowallGuid;
    nx::Uuid instanceGuid;
    std::map<QString, QString> params;
};
#define VideowallControlMessageData_Fields (operation)(videowallGuid)(instanceGuid)(params)
NX_VMS_API_DECLARE_STRUCT(VideowallControlMessageData)
NX_REFLECTION_INSTRUMENT(VideowallControlMessageData, VideowallControlMessageData_Fields)

} // namespace api
} // namespace vms
} // namespace nx
