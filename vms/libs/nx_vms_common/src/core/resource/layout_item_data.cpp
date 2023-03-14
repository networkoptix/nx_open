// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "layout_item_data.h"

#include <nx/reflect/compare.h>

namespace nx::vms::common {

bool LayoutItemData::operator==(const LayoutItemData& other) const
{
    return nx::reflect::equals(*this, other);
}

} // namespace nx::vms::common
