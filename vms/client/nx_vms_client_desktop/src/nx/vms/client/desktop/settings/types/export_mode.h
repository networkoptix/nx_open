// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/reflect/enum_instrument.h>

namespace nx::vms::client::desktop {

enum class ExportMode { media, layout };

template<typename Visitor>
constexpr auto nxReflectVisitAllEnumItems(ExportMode*, Visitor&& visitor)
{
    using Item = nx::reflect::enumeration::Item<ExportMode>;
    return visitor(
        Item{ExportMode::media, "media"},
        Item{ExportMode::layout, "layout"},
        // Compatibility values to import settings from 5.1.
        Item{ExportMode::media, "Media"},
        Item{ExportMode::layout, "Layout"}
    );
}


} // namespace nx::vms::client::desktop
