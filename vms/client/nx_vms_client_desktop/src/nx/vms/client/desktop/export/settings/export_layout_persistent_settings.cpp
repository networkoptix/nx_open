#include "export_layout_persistent_settings.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::client::desktop {

void ExportLayoutPersistentSettings::updateRuntimeSettings(
    ExportLayoutSettings& runtimeSettings) const
{
    runtimeSettings.readOnly = readOnly;
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES((ExportLayoutPersistentSettings), (json), _Fields)

} // namespace nx::vms::client::desktop
