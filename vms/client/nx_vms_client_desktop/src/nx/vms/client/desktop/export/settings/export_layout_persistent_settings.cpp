// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "export_layout_persistent_settings.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::client::desktop {

void ExportLayoutPersistentSettings::updateRuntimeSettings(
    ExportLayoutSettings& runtimeSettings) const
{
    runtimeSettings.readOnly = readOnly;
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    ExportLayoutPersistentSettings, (json), ExportLayoutPersistentSettings_Fields)

} // namespace nx::vms::client::desktop
