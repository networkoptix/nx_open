#include "device_storage_info.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::statistics {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    StreamStorageInfo,
    DEVICE_STORAGE_INFO_FUNCTIONS,
    StreamStorageInfo_Fields)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    CountAndAverageSize,
    DEVICE_STORAGE_INFO_FUNCTIONS,
    CountAndAverageSize_Fields)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    DeviceStorageInfo,
    DEVICE_STORAGE_INFO_FUNCTIONS,
    DeviceStorageInfo_Fields)

} // namespace nx::vms::statistics
