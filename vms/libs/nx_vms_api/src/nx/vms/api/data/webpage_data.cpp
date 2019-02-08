#include "webpage_data.h"

#include <nx/fusion/model_functions.h>

namespace nx {
namespace vms {
namespace api {

const QString WebPageData::kResourceTypeName = lit("WebPage");
const QnUuid WebPageData::kResourceTypeId =
    ResourceData::getFixedTypeId(WebPageData::kResourceTypeName);

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (WebPageData),
    (eq)(ubjson)(xml)(json)(sql_record)(csv_record),
    _Fields)

} // namespace api
} // namespace vms
} // namespace nx
