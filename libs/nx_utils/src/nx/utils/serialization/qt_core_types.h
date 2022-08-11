// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/qnamespace.h>

#include <nx/reflect/enum_instrument.h>
#include <nx/reflect/instrument.h>

#include "flags.h"

namespace Qt {

// Disable deprecation warnings.
#if defined(__GNUC__)
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#elif defined(_MSC_VER)
    #pragma warning(push)
    #pragma warning(disable: 4996)
#endif

NX_REFLECTION_INSTRUMENT_ENUM(Orientation, Horizontal, Vertical)

NX_REFLECTION_INSTRUMENT_ENUM(AlignmentFlag,
    AlignLeft,
    AlignLeading,
    AlignRight,
    AlignTrailing,
    AlignHCenter,
    AlignJustify,
    AlignAbsolute,
    AlignHorizontal_Mask,
    AlignTop,
    AlignBottom,
    AlignVCenter,
    AlignBaseline,
    AlignVertical_Mask,
    AlignCenter
)

NX_REFLECTION_INSTRUMENT_ENUM(DateFormat,
    TextDate,
    ISODate,
    SystemLocaleDate,
    LocalDate,
    LocaleDate,
    SystemLocaleShortDate,
    SystemLocaleLongDate,
    DefaultLocaleShortDate,
    DefaultLocaleLongDate,
    RFC2822Date,
    ISODateWithMs
)

template<typename Visitor>
constexpr auto nxReflectVisitAllEnumItems(Qt::SortOrder*, Visitor&& visitor)
{
    using Item = nx::reflect::enumeration::Item<Qt::SortOrder>;
    return visitor(
        Item{Qt::SortOrder::AscendingOrder, "asc"},
        Item{Qt::SortOrder::DescendingOrder, "desc"}
    );
}

#if defined(__GNUC__)
    #pragma GCC diagnostic pop
#elif defined(_MSC_VER)
    #pragma warning(pop)
#endif

} // namespace Qt
