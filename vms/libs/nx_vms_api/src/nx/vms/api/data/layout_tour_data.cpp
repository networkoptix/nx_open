// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "layout_tour_data.h"

#include <nx/fusion/model_functions.h>

namespace nx {
namespace vms {
namespace api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    LayoutTourItemData, (ubjson)(xml)(json)(sql_record)(csv_record), LayoutTourItemData_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    LayoutTourData, (ubjson)(xml)(json)(sql_record)(csv_record), LayoutTourData_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    LayoutTourSettings, (ubjson)(xml)(json)(csv_record), LayoutTourSettings_Fields)

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
