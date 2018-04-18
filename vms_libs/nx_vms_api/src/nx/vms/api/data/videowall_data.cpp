#include "videowall_data.h"

#include <nx/fusion/model_functions.h>

namespace nx {
namespace vms {
namespace api {

#define VIDEOWALL_TYPES \
    (VideowallItemData) \
    (VideowallScreenData) \
    (VideowallMatrixItemData) \
    (VideowallMatrixData) \
    (VideowallData) \
    (VideowallControlMessageData)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    VIDEOWALL_TYPES,
    (eq)(ubjson)(xml)(json)(sql_record)(csv_record),
    _Fields)

} // namespace api
} // namespace vms
} // namespace nx
