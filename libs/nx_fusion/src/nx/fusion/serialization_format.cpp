#include "serialization_format.h"

#include <nx/utils/log/assert.h>

#include "model_functions.h"

namespace Qn {

const char* serializationFormatToHttpContentType(SerializationFormat format)
{
    switch (format)
    {
        case JsonFormat:
            return "application/json";
        case UbjsonFormat:
            return "application/ubjson";
        case BnsFormat:
            return "application/octet-stream";
        case CsvFormat:
            return "text/csv";
        case XmlFormat:
            return "application/xml";
        case CompressedPeriodsFormat:
            return "application/x-periods";
        case UrlQueryFormat:
            return "application/x-url-query";
        default:
            NX_ASSERT(false);
            return "unsupported";
    }
}

SerializationFormat serializationFormatFromHttpContentType(const QByteArray& httpContentType)
{
    if (httpContentType == "application/json")
        return JsonFormat;
    if (httpContentType == "application/ubjson")
        return UbjsonFormat;
    if (httpContentType == "application/octet-stream")
        return BnsFormat;
    if (httpContentType == "text/csv")
        return CsvFormat;
    if (httpContentType == "application/xml")
        return XmlFormat;
    if (httpContentType == "application/x-periods")
        return CompressedPeriodsFormat;
    if (httpContentType == "application/x-url-query")
        return UrlQueryFormat;

    return UnsupportedFormat;
}

} // namespace Qn

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(Qn, SerializationFormat,
    (Qn::JsonFormat,        "json")
    (Qn::UbjsonFormat,      "ubjson")
    (Qn::BnsFormat,         "bns")
    (Qn::CsvFormat,         "csv")
    (Qn::XmlFormat,         "xml")
    (Qn::CompressedPeriodsFormat, "periods")
)
