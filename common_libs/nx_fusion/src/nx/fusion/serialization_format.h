#pragma once

#include <nx/utils/log/assert.h>
#include <nx/utils/literal.h>
#include <nx/utils/string.h>

#include "model_functions_fwd.h"
////#include "model_functions.h"

namespace Qn {

enum SerializationFormat
{
    JsonFormat = 0,
    UbjsonFormat = 1,
    BnsFormat = 2,
    CsvFormat = 3,
    XmlFormat = 4,
    CompressedPeriodsFormat = 5, //< Used for chunks data only.
    UrlQueryFormat = 6, //< May be added in the future for parsing "name1=val1&name2=val2".

    UnsupportedFormat = -1
};
QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(SerializationFormat)

const char* serializationFormatToHttpContentType(SerializationFormat format);
SerializationFormat serializationFormatFromHttpContentType(
    const QByteArray& httpContentType);

template<typename OutputData>
QByteArray serialized(
    const OutputData& outputData, Qn::SerializationFormat format, bool extraFormatting)
{
    switch(format)
    {
        case Qn::UbjsonFormat:
            return QnUbjson::serialized(outputData);

        case Qn::JsonFormat:
        case Qn::UnsupportedFormat:
        {
            QByteArray result = QJson::serialized(outputData);
            if (extraFormatting)
                result = nx::utils::formatJsonString(result);
            return result;
        }

        case Qn::CsvFormat:
            return QnCsv::serialized(outputData);

        case Qn::XmlFormat:
        {
            QByteArray result = QnXml::serialized(outputData, lit("reply"));
            return result;
        }

        default:
            NX_ASSERT(false);
            return QJson::serialized(outputData);
    }
}

} // namespace Qn

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (Qn::SerializationFormat),
    (metatype)(lexical))
