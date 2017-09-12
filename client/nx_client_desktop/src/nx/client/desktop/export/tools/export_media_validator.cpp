#include "export_media_validator.h"

#include <nx/client/desktop/common/utils/filesystem.h>
#include <nx/client/desktop/export/data/export_media_settings.h>
#include <nx/client/desktop/export/data/export_layout_settings.h>

namespace nx {
namespace client {
namespace desktop {

ExportMediaValidator::Results ExportMediaValidator::validateSettings(
    const ExportMediaSettings& settings)
{
    if (settings.fileName.extension == FileExtension::avi)
    {

    }

    return Results();
}

ExportMediaValidator::Results ExportMediaValidator::validateSettings(
    const ExportLayoutSettings& settings)
{
    if (FileExtensionUtils::isExecutable(settings.filename.extension))
    {

    }

    return Results();
}

} // namespace desktop
} // namespace client
} // namespace nx
