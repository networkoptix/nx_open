// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QMetaType>

#include <nx/reflect/enum_instrument.h>

namespace Qn {

enum SerializationFormat: int
{
    JsonFormat = 0,
    UbjsonFormat = 1,
    // BnsFormat = 2,
    CsvFormat = 3,
    XmlFormat = 4,
    CompressedPeriodsFormat = 5, //< Used for chunks data only.
    UrlQueryFormat = 6, //< May be added in the future for parsing "name1=val1&name2=val2".
    UrlEncodedFormat = 7,

    UnsupportedFormat = -1
};

template<typename Visitor>
constexpr auto nxReflectVisitAllEnumItems(SerializationFormat*, Visitor&& visitor)
{
    using Item = nx::reflect::enumeration::Item<SerializationFormat>;
    return visitor(
        Item{JsonFormat, "json"},
        Item{UbjsonFormat, "ubjson"},
        Item{CsvFormat, "csv"},
        Item{XmlFormat, "xml"},
        Item{CompressedPeriodsFormat, "periods"},
        Item{UrlEncodedFormat, "urlencoded"}
    );
}

NX_UTILS_API const char* serializationFormatToHttpContentType(SerializationFormat format);

NX_UTILS_API SerializationFormat serializationFormatFromHttpContentType(
    const std::string_view& httpContentType);

NX_UTILS_API SerializationFormat serializationFormatFromHttpContentType(
    const QByteArray& httpContentType);

} // namespace Qn

Q_DECLARE_METATYPE(Qn::SerializationFormat)
