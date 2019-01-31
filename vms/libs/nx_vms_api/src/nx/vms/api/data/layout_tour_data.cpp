#include "layout_tour_data.h"

#include <nx/fusion/model_functions.h>

namespace nx {
namespace vms {
namespace api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (LayoutTourItemData)(LayoutTourData),
    (eq)(ubjson)(xml)(json)(sql_record)(csv_record),
    _Fields)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (LayoutTourSettings),
    (eq)(ubjson)(xml)(json)(csv_record),
    _Fields)

} // namespace api
} // namespace vms
} // namespace nx

void serialize_field(const nx::vms::api::LayoutTourSettings& settings, QVariant* target)
{
    serialize_field(QJson::serialized(settings), target);
}

void deserialize_field(const QVariant& value, nx::vms::api::LayoutTourSettings* target)
{
    QByteArray tmp;
    deserialize_field(value, &tmp);
    QJson::deserialize(tmp, target);
}
