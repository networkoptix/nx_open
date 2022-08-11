// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QMetaType>

#include <nx/reflect/enum_instrument.h>

namespace nx::core::transcoding {

enum class TimestampFormat
{
    longDate,
    shortDate,
    RFC2822Date,
    ISODate
};

// Support custom deserialization from previously saved values.
template<typename Visitor>
constexpr auto nxReflectVisitAllEnumItems(TimestampFormat*, Visitor&& visitor)
{
    using Item = nx::reflect::enumeration::Item<TimestampFormat>;
    return visitor(
        Item{TimestampFormat::longDate, "DefaultLocaleLongDate"},
        Item{TimestampFormat::shortDate, "DefaultLocaleShortDate"},
        Item{TimestampFormat::RFC2822Date, "RFC2822Date"},
        Item{TimestampFormat::ISODate, "ISODate"}
    );
}

} // namespace nx::core::transcoding

Q_DECLARE_METATYPE(nx::core::transcoding::TimestampFormat)
