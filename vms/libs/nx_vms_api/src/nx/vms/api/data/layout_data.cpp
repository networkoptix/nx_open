#include "layout_data.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api {

const QString LayoutData::kResourceTypeName = lit("Layout");
const QnUuid LayoutData::kResourceTypeId =
    ResourceData::getFixedTypeId(LayoutData::kResourceTypeName);

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (LayoutItemData)(LayoutData),
    (eq)(ubjson)(xml)(json)(sql_record)(csv_record),
    _Fields)

} // namespace nx::vms::api
