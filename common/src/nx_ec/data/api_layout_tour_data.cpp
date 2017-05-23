#include "api_layout_tour_data.h"

#include <nx/fusion/model_functions.h>

namespace ec2 {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
     (ApiLayoutTourItemData)(ApiLayoutTourData),
     (ubjson)(xml)(json)(sql_record)(csv_record),
     _Fields)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (ApiLayoutTourSettings),
    (ubjson)(xml)(json)(csv_record),
    _Fields)

bool ApiLayoutTourItemData::operator==(const ApiLayoutTourItemData& rhs) const
{
     return resourceId == rhs.resourceId
         && delayMs == rhs.delayMs;
}

bool ApiLayoutTourSettings::operator==(const ApiLayoutTourSettings& rhs) const
{
    return manual == rhs.manual;
}

bool ApiLayoutTourSettings::operator!=(const ApiLayoutTourSettings& rhs) const
{
    return !(*this == rhs);
}

bool ApiLayoutTourData::isValid() const
{
    return !id.isNull();
}

bool ApiLayoutTourData::operator!=(const ApiLayoutTourData& rhs) const
{
    return !(*this == rhs);
}

bool ApiLayoutTourData::operator==(const ApiLayoutTourData& rhs) const
{
    return id == rhs.id
        && name == rhs.name
        && items == rhs.items
        && settings == rhs.settings;
}

} // namespace ec2

void serialize_field(const ec2::ApiLayoutTourSettings& settings, QVariant* target)
{
    serialize_field(QJson::serialized(settings), target);
}

void deserialize_field(const QVariant& value, ec2::ApiLayoutTourSettings* target)
{
    QByteArray tmp;
    deserialize_field(value, &tmp);
    QJson::deserialize(tmp, target);
}
