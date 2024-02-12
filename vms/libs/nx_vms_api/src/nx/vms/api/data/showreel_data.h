// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>

#include "id_data.h"

namespace nx::vms::api {

struct NX_VMS_API ShowreelItemData
{
    nx::Uuid resourceId;

    /**%apidoc[opt] */
    int delayMs = 0;

    ShowreelItemData() = default;
    ShowreelItemData(const nx::Uuid& resourceId, int delayMs):
        resourceId(resourceId), delayMs(delayMs) {}

    bool operator==(const ShowreelItemData& other) const = default;
};
#define ShowreelItemData_Fields (resourceId)(delayMs)
NX_VMS_API_DECLARE_STRUCT_AND_LIST(ShowreelItemData)

struct NX_VMS_API ShowreelSettings
{
    bool manual = false;

    bool operator==(const ShowreelSettings& other) const = default;
};
#define ShowreelSettings_Fields (manual)
NX_VMS_API_DECLARE_STRUCT(ShowreelSettings)

struct NX_VMS_API ShowreelData: IdData
{
    nx::Uuid parentId;

    /**%apidoc
     * %example Showreel
     */
    QString name;

    ShowreelItemDataList items;
    ShowreelSettings settings;

    bool isValid() const { return !id.isNull(); }

    bool operator==(const ShowreelData& other) const = default;
};
#define ShowreelData_Fields IdData_Fields (parentId)(name)(items)(settings)
NX_VMS_API_DECLARE_STRUCT_AND_LIST(ShowreelData)

} // namespace nx::vms::api
