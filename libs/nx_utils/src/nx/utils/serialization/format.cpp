// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "format.h"

#include <nx/reflect/string_conversion.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/string.h>

namespace Qn {

const char* serializationFormatToHttpContentType(SerializationFormat format)
{
    switch (format)
    {
        case SerializationFormat::json:
            return "application/json";
        case SerializationFormat::ubjson:
            return "application/ubjson";
        case SerializationFormat::csv:
            return "text/csv";
        case SerializationFormat::xml:
            return "application/xml";
        case SerializationFormat::compressedPeriods:
            return "application/x-periods";
        case SerializationFormat::urlQuery:
            return "application/x-url-query";
        case SerializationFormat::urlEncoded:
            return "application/x-www-form-urlencoded";
        default:
            NX_ASSERT(false, "Value: %1", static_cast<int>(format));
            return "unsupported";
    }
}

SerializationFormat serializationFormatFromHttpContentType(
    const std::string_view& httpContentType)
{
    if (nx::utils::contains(httpContentType, "application/json") || nx::utils::contains(httpContentType, "*/*"))
        return SerializationFormat::json;
    if (nx::utils::contains(httpContentType, "application/ubjson"))
        return SerializationFormat::ubjson;
    if (nx::utils::contains(httpContentType, "application/xml"))
        return SerializationFormat::xml;
    if (nx::utils::contains(httpContentType, "text/csv"))
        return SerializationFormat::csv;
    if (nx::utils::contains(httpContentType, "application/x-periods"))
        return SerializationFormat::compressedPeriods;
    if (nx::utils::contains(httpContentType, "application/x-url-query"))
        return SerializationFormat::urlQuery;
    if (nx::utils::contains(httpContentType, "application/x-www-form-urlencoded"))
        return SerializationFormat::urlEncoded;

    return SerializationFormat::unsupported;
}

SerializationFormat serializationFormatFromHttpContentType(
    const QByteArray& httpContentType)
{
    return serializationFormatFromHttpContentType(
        std::string_view(httpContentType.data(), httpContentType.size()));
}

QDebug operator<<(QDebug d, const SerializationFormat& value)
{
    return d << nx::reflect::toString(value);
}


} // namespace Qn
