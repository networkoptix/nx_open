// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/reflect/instrument.h>
#include <nx/vms/api/data/layout_data.h>
#include <nx/vms/api/data/resource_data.h>
#include <nx/vms/client/desktop/resource/resource_fwd.h>

namespace nx::vms::client::desktop {

struct CrossSystemLayoutItemData: nx::vms::api::LayoutItemData
{
    QString name;
};
#define CrossSystemLayoutItemData_Fields LayoutItemData_Fields (name)
NX_REFLECTION_INSTRUMENT(CrossSystemLayoutItemData, CrossSystemLayoutItemData_Fields)

struct CrossSystemLayoutData
{
    QnUuid id;
    QString name;
    float cellAspectRatio = 0;
    float cellSpacing = nx::vms::api::LayoutData::kDefaultCellSpacing;
    bool locked = false;
    qint32 fixedWidth = 0;
    qint32 fixedHeight = 0;

    std::vector<CrossSystemLayoutItemData> items;
    nx::vms::api::ResourceParamDataList parameters;
};
#define CrossSystemLayoutData_Fields (id)(name)(cellAspectRatio)(cellSpacing) \
    (locked)(fixedWidth)(fixedHeight)(items)(parameters)
NX_REFLECTION_INSTRUMENT(CrossSystemLayoutData, CrossSystemLayoutData_Fields)

void fromDataToResource(
    const CrossSystemLayoutData& data,
    const CrossSystemLayoutResourcePtr& resource);

void fromResourceToData(
    const CrossSystemLayoutResourcePtr& resource,
    CrossSystemLayoutData& data);

} // namespace nx::vms::client::desktop
