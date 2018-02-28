#include "plugin_setting.h"

#include <nx/fusion/model_functions.h>

namespace nx {
namespace mediaserver {
namespace metadata {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    PluginSetting,
    (json),
    PluginSetting_Fields,
    (brief, true));

} // namespace metadata
} // namespace mediaserver
} // namespace nx
