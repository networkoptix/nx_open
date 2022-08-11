// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "export_layout_persistent_settings.h"

namespace nx::vms::client::desktop {

void ExportLayoutPersistentSettings::updateRuntimeSettings(
    ExportLayoutSettings& runtimeSettings) const
{
    runtimeSettings.readOnly = readOnly;
}

} // namespace nx::vms::client::desktop
