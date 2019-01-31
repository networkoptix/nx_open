#pragma once

#include <nx/fusion/model_functions_fwd.h>
#include <nx/vms/client/desktop/common/utils/filesystem.h>
#include <nx/vms/client/desktop/export/data/export_layout_settings.h>

namespace nx::vms::client::desktop {

struct ExportLayoutPersistentSettings
{
    bool readOnly = false;
    QString fileFormat;

    void updateRuntimeSettings(ExportLayoutSettings& runtimeSettings) const;
};
#define ExportLayoutPersistentSettings_Fields (readOnly)(fileFormat)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((ExportLayoutPersistentSettings), (json))

} // namespace nx::vms::client::desktop

Q_DECLARE_METATYPE(nx::vms::client::desktop::ExportLayoutPersistentSettings)
