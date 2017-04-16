#include "api_layout_tour_data.h"

#include <nx/fusion/model_functions.h>

namespace ec2 {

 QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
     (ApiLayoutTourItemData)(ApiLayoutTourData),
     (ubjson)(xml)(json)(sql_record)(csv_record),
     _Fields)

bool ApiLayoutTourItemData::operator==(const ApiLayoutTourItemData& rhs) const
{
     return layoutId == rhs.layoutId
         && delayMs == rhs.delayMs;
}

bool ApiLayoutTourData::isValid() const
{
    return !id.isNull();
}

bool ApiLayoutTourData::operator!=(const ApiLayoutTourData& rhs) const
{
    return !((*this) == rhs);
}

bool ApiLayoutTourData::operator==(const ApiLayoutTourData& rhs) const
{
    return id == rhs.id
        && name == rhs.name
        && items == rhs.items;
}

} // namespace ec2
