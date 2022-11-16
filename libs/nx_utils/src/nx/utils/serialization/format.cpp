// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "format.h"

#include <nx/utils/log/assert.h>
#include <nx/utils/string.h>

namespace Qn {

const char* serializationFormatToHttpContentType(SerializationFormat format)
{
    switch (format)
    {
        case JsonFormat:
            return "application/json";
        case UbjsonFormat:
            return "application/ubjson";
        case CsvFormat:
            return "text/csv";
        case XmlFormat:
            return "application/xml";
        case CompressedPeriodsFormat:
            return "application/x-periods";
        case UrlQueryFormat:
            return "application/x-url-query";
        case UrlEncodedFormat:
            return "application/x-www-form-urlencoded";
        default:
            NX_ASSERT(false);
            return "unsupported";
    }
}

SerializationFormat serializationFormatFromHttpContentType(
    const std::string_view& httpContentType)
{
    if (nx::utils::contains(httpContentType, "application/json") || nx::utils::contains(httpContentType, "*/*"))
        return JsonFormat;
    if (nx::utils::contains(httpContentType, "application/ubjson"))
        return UbjsonFormat;
    if (nx::utils::contains(httpContentType, "application/xml"))
        return XmlFormat;
    if (nx::utils::contains(httpContentType, "text/csv"))
        return CsvFormat;
    if (nx::utils::contains(httpContentType, "application/x-periods"))
        return CompressedPeriodsFormat;
    if (nx::utils::contains(httpContentType, "application/x-url-query"))
        return UrlQueryFormat;
    if (nx::utils::contains(httpContentType, "application/x-www-form-urlencoded"))
        return UrlEncodedFormat;

    return UnsupportedFormat;
}

SerializationFormat serializationFormatFromHttpContentType(
    const QByteArray& httpContentType)
{
    return serializationFormatFromHttpContentType(
        std::string_view(httpContentType.data(), httpContentType.size()));
}

} // namespace Qn
