// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "id_data.h"

#include <QtCore/QString>

namespace nx {
namespace vms {
namespace api {

struct NX_VMS_API LayoutTourItemData
{
    QnUuid resourceId;

    /**%apidoc[opt] */
    int delayMs = 0;

    LayoutTourItemData() = default;
    LayoutTourItemData(const QnUuid& resourceId, int delayMs):
        resourceId(resourceId), delayMs(delayMs) {}

    bool operator==(const LayoutTourItemData& other) const = default;
};
#define LayoutTourItemData_Fields (resourceId)(delayMs)
NX_VMS_API_DECLARE_STRUCT_AND_LIST(LayoutTourItemData)

struct NX_VMS_API LayoutTourSettings
{
    bool manual = false;

    bool operator==(const LayoutTourSettings& other) const = default;
};
#define LayoutTourSettings_Fields (manual)
NX_VMS_API_DECLARE_STRUCT(LayoutTourSettings)

struct NX_VMS_API LayoutTourData: IdData
{
    QnUuid parentId;

    /**%apidoc
     * %example Layout Tour
     */
    QString name;

    LayoutTourItemDataList items;
    LayoutTourSettings settings;

    bool isValid() const { return !id.isNull(); }

    bool operator==(const LayoutTourData& other) const = default;
};
#define LayoutTourData_Fields IdData_Fields (parentId)(name)(items)(settings)
NX_VMS_API_DECLARE_STRUCT_AND_LIST(LayoutTourData)

} // namespace api
} // namespace vms
} // namespace nx

