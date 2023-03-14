// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/reflect/instrument.h>
#include <nx/vms/client/desktop/common/utils/filesystem.h>
#include <nx/vms/client/desktop/export/data/export_layout_settings.h>

namespace nx::vms::client::desktop {

struct ExportLayoutPersistentSettings
{
    bool readOnly = false;
    QString fileFormat;

    void updateRuntimeSettings(ExportLayoutSettings& runtimeSettings) const;

    bool operator==(const ExportLayoutPersistentSettings&) const = default;
};
#define ExportLayoutPersistentSettings_Fields (readOnly)(fileFormat)
NX_REFLECTION_INSTRUMENT(ExportLayoutPersistentSettings, ExportLayoutPersistentSettings_Fields)

} // namespace nx::vms::client::desktop
