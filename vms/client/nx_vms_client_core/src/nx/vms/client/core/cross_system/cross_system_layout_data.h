// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/resource_fwd.h>
#include <nx/reflect/instrument.h>
#include <nx/vms/api/data/layout_data.h>

namespace nx::vms::client::core {

struct CrossSystemLayoutItemData: nx::vms::api::LayoutItemData
{
    QString name;
};
#define CrossSystemLayoutItemData_Fields LayoutItemData_Fields (name)
NX_REFLECTION_INSTRUMENT(CrossSystemLayoutItemData, CrossSystemLayoutItemData_Fields)

struct NX_VMS_CLIENT_CORE_API CrossSystemLayoutData
{
    nx::Uuid id;
    QString name;
    float cellAspectRatio = 0;
    float cellSpacing = nx::vms::api::LayoutData::kDefaultCellSpacing;
    bool locked = false;
    qint32 fixedWidth = 0;
    qint32 fixedHeight = 0;
    std::vector<CrossSystemLayoutItemData> items;
    QString customGroupId;
};
#define CrossSystemLayoutData_Fields (id)(name)(cellAspectRatio)(cellSpacing) \
    (locked)(fixedWidth)(fixedHeight)(items)(customGroupId)
NX_REFLECTION_INSTRUMENT(CrossSystemLayoutData, CrossSystemLayoutData_Fields)

NX_VMS_CLIENT_CORE_API void fromDataToResource(
    const CrossSystemLayoutData& data,
    const QnLayoutResourcePtr& resource);

NX_VMS_CLIENT_CORE_API void fromResourceToData(
    const QnLayoutResourcePtr& resource,
    CrossSystemLayoutData& data);

} // namespace nx::vms::client::core
