// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string_view>

#include <QtCore/QByteArray>
#include <QtCore/QDebug>

#include <nx/reflect/enum_instrument.h>

namespace Qn {

enum class SerializationFormat: int
{
    json = 0,
    ubjson = 1,
    // bns = 2,
    csv = 3,
    xml = 4,

    /**%apidoc
     * Used for chunks data only.
     * %caption periods
     */
    compressedPeriods = 5,

    /**%apidoc
     * May be added in the future for parsing "name1=val1&name2=val2".
     * %caption urlquery
     */
    urlQuery = 6,

    /**%apidoc
     * %caption urlencoded
     */
    urlEncoded = 7,

    /**%apidoc[unused] */
    unsupported = -1
};

template<typename Visitor>
constexpr auto nxReflectVisitAllEnumItems(SerializationFormat*, Visitor&& visitor)
{
    using Item = nx::reflect::enumeration::Item<SerializationFormat>;
    return visitor(
        Item{SerializationFormat::json, "json"},
        Item{SerializationFormat::ubjson, "ubjson"},
        Item{SerializationFormat::csv, "csv"},
        Item{SerializationFormat::xml, "xml"},
        Item{SerializationFormat::compressedPeriods, "periods"},
        Item{SerializationFormat::urlQuery, "urlquery"},
        Item{SerializationFormat::urlEncoded, "urlencoded"}
    );
}

NX_UTILS_API const char* serializationFormatToHttpContentType(SerializationFormat format);

NX_UTILS_API SerializationFormat serializationFormatFromHttpContentType(
    const std::string_view& httpContentType);

NX_UTILS_API SerializationFormat serializationFormatFromHttpContentType(
    const QByteArray& httpContentType);

NX_UTILS_API QDebug operator<<(QDebug, const SerializationFormat&);

} // namespace Qn
