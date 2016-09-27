#include "transaction_timestamp.h"

#include <nx/fusion/model_functions.h>
#include <nx/fusion/serialization/binary.h>
#include <nx/fusion/serialization/json.h>
#include <nx/fusion/serialization/xml.h>
#include <nx/fusion/serialization/csv.h>
#include <nx/fusion/serialization/ubjson.h>

namespace ec2 {

Timestamp operator+(const Timestamp& left, std::uint64_t right)
{
    Timestamp result = left;
    result += right;
    return result;
}

QString toString(const Timestamp& val)
{
    return lit("(%1, %2)").arg(val.sequence).arg(val.ticks);
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    Timestamp,
    (json)(ubjson)(xml)(csv_record),
    Timestamp_Fields,
    (optional, false))

} // namespace ec2
