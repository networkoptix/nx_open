#include "layout_data.h"

#include <nx/fusion/model_functions.h>

namespace nx {
namespace vms {
namespace api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (LayoutItemData)(LayoutData),
    (eq)(ubjson)(xml)(json)(sql_record)(csv_record),
    _Fields)

} // namespace api
} // namespace vms
} // namespace nx
