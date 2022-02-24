// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "layout_data.h"

#include <nx/fusion/model_functions.h>
#include <nx/reflect/compare.h>

namespace nx::vms::api {

const QString LayoutData::kResourceTypeName = lit("Layout");
const QnUuid LayoutData::kResourceTypeId =
    ResourceData::getFixedTypeId(LayoutData::kResourceTypeName);

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    LayoutItemData, (ubjson)(xml)(json)(sql_record)(csv_record), LayoutItemData_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    LayoutData, (ubjson)(xml)(json)(sql_record)(csv_record), LayoutData_Fields)

bool LayoutItemData::operator==(const LayoutItemData& other) const
{
    return nx::reflect::equals(*this, other);
}

bool LayoutData::operator==(const LayoutData& other) const
{
    return nx::reflect::equals(*this, other);
}

} // namespace nx::vms::api
