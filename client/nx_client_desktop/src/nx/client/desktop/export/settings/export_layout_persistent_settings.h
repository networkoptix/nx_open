#pragma once

#include <nx/fusion/model_functions_fwd.h>
#include <nx/client/desktop/common/utils/filesystem.h>
#include <nx/client/desktop/export/data/export_layout_settings.h>

namespace nx {
namespace client {
namespace desktop {

struct ExportLayoutPersistentSettings
{
    bool readOnly = false;
    FileExtension fileFormat = FileExtension::nov;

    void updateRuntimeSettings(ExportLayoutSettings& runtimeSettings) const;
};
#define ExportLayoutPersistentSettings_Fields (readOnly)(fileFormat)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((ExportLayoutPersistentSettings), (json))

} // namespace desktop
} // namespace client
} // namespace nx

Q_DECLARE_METATYPE(nx::client::desktop::ExportLayoutPersistentSettings)
