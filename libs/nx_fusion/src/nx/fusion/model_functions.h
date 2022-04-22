// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/uuid.h>
#include <nx/utils/string.h>
#include <nx/fusion/fusion/fusion_adaptor.h>
#include <nx/fusion/serialization/data_stream_macros.h>
#include <nx/fusion/serialization/debug_macros.h>
#include <nx/fusion/serialization/csv_functions.h>
#include <nx/fusion/serialization/json_functions.h>
#include <nx/fusion/serialization/lexical_functions.h>
#include <nx/fusion/serialization/sql_functions.h>
#include <nx/fusion/serialization/ubjson_functions.h>
#include <nx/fusion/serialization/xml_functions.h>
#include <nx/fusion/serialization_format.h>
#include <nx/utils/math/fuzzy.h>

namespace QnHashAutomation {
    struct Visitor {
    public:
        Visitor(uint seed): m_result(seed) {}

        template<class T, class Access>
        bool operator()(const T &value, const Access &access) {
            using namespace QnFusion;

            m_result ^= qHash(invoke(access(getter), value));
            return true;
        }

        uint result() const {
            return m_result;
        }

    private:
        uint m_result;
    };

} // namespace QnHashAutomation


/**
 * This macro generates <tt>qHash</tt> function for the given fusion-adapted type.
 *
 * \param TYPE                          Fusion-adapted type to define hash function for.
 * \param PREFIX                        Optional function definition prefix, e.g. <tt>inline</tt>.
 */
#define QN_FUSION_DEFINE_FUNCTIONS_hash(TYPE, ... /* PREFIX */)                 \
__VA_ARGS__ uint qHash(const TYPE &value, uint seed) {                          \
    QnHashAutomation::Visitor visitor(seed);                                    \
    QnFusion::visit_members(value, visitor);                                    \
    return visitor.result();                                                    \
}

namespace Qn {

template<typename OutputData>
std::optional<QByteArray> trySerialize(const OutputData& outputData,
    Qn::SerializationFormat format, bool extraFormatting)
{
    switch (format)
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
            QByteArray result = QnXml::serialized(outputData, QStringLiteral("reply"));
            return result;
        }
        default:
            return {};
    }
}

template<typename OutputData>
QByteArray serialized(
    const OutputData& outputData, Qn::SerializationFormat format, bool extraFormatting)
{
    auto serializedData = trySerialize(outputData, format, extraFormatting);
    NX_ASSERT(serializedData.has_value());
    return serializedData.has_value() ? *serializedData : QByteArray();
}

} // namespace Qn
