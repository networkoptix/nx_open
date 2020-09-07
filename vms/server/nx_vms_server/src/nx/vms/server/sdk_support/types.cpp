#include "types.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::server::sdk_support {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(SdkSettingsResponse, (json), (values)(errors)(model)(sdkError))

} // namespace nx::vms::server::sdk_support