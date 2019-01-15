#include "dewarping_data.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(DewarpingData,
    (eq)(ubjson)(json)(xml)(csv_record),
    DewarpingData_Fields)

QByteArray DewarpingData::toByteArray() const
{
    return QJson::serialized(*this);
}

DewarpingData DewarpingData::fromByteArray(const QByteArray& data)
{
    return QJson::deserialized<DewarpingData>(data);
}

} // namespace nx::vms::api

void serialize_field(const nx::vms::api::DewarpingData& data, QVariant* target)
{
    serialize_field(data.toByteArray(), target);
}

void deserialize_field(const QVariant& value, nx::vms::api::DewarpingData* target)
{
    QByteArray tmp;
    deserialize_field(value, &tmp);
    *target = nx::vms::api::DewarpingData::fromByteArray(tmp);
}
