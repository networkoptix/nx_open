#pragma once

#include "model_functions_fwd.h"

namespace Qn {

enum SerializationFormat
{
    JsonFormat = 0,
    UbjsonFormat = 1,
    BnsFormat = 2,
    CsvFormat = 3,
    XmlFormat = 4,
    CompressedPeriodsFormat = 5, // used for chunks data only
    UrlQueryFormat = 6,     //will be added in future for parsing url query (e.g., name1=val1&name2=val2)

    UnsupportedFormat = -1
};
QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(SerializationFormat)

const char* serializationFormatToHttpContentType(SerializationFormat format);
SerializationFormat serializationFormatFromHttpContentType(
    const QByteArray& httpContentType);

} // namespace Qn

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (Qn::SerializationFormat),
    (metatype)(lexical))
