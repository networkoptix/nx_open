#include "videowall_data.h"

#include <nx/fusion/model_functions.h>

namespace nx {
namespace vms {
namespace api {

const QString VideowallData::kResourceTypeName = lit("Videowall");
const QnUuid VideowallData::kResourceTypeId =
    ResourceData::getFixedTypeId(VideowallData::kResourceTypeName);

#define VIDEOWALL_TYPES \
    (VideowallItemData) \
    (VideowallScreenData) \
    (VideowallMatrixItemData) \
    (VideowallMatrixData) \
    (VideowallData)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    VIDEOWALL_TYPES,
    (eq)(ubjson)(xml)(json)(sql_record)(csv_record),
    _Fields)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (VideowallControlMessageData),
    (ubjson)(json),
    _Fields)

} // namespace api
} // namespace vms
} // namespace nx
