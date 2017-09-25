#include "media_persistent.h"

#include <client/client_meta_types.h>
#include <nx/fusion/model_functions.h>

namespace nx {
namespace client {
namespace desktop {
namespace settings {

ExportImageOverlayPersistent::ExportImageOverlayPersistent()
{
    image = QImage(lit(":/logo.png"));
    overlayWidth = image.width();
}

ExportBookmarkOverlayPersistent::ExportBookmarkOverlayPersistent()
{
    foreground = Qt::white;
    background = QColor(0x2E, 0x69, 0x96, 0xB3);
}

ExportOverlaySettings* ExportMediaPersistent::overlaySettings(ExportOverlayType type)
{
    switch (type)
    {
        case settings::ExportOverlayType::timestamp:
            return &timestampOverlay;
        case settings::ExportOverlayType::image:
            return &imageOverlay;
        case settings::ExportOverlayType::text:
            return &textOverlay;
        case settings::ExportOverlayType::bookmark:
            return &bookmarkOverlay;
        default:
            return static_cast<ExportOverlaySettings*>(nullptr);
    }
}

void ExportMediaPersistent::updateRuntimeSettings()
{
    transcodingSettings.overlays.clear();
    transcodingSettings.overlays.reserve(usedOverlays.size());

    const auto createRuntimeSettings =
        [this](ExportOverlayType type) -> QSharedPointer<ExportOverlaySettings>
        {
            using res_type = QSharedPointer<ExportOverlaySettings>;
            switch (type)
            {
                case settings::ExportOverlayType::timestamp:
                    return res_type(new ExportTimestampOverlaySettings(timestampOverlay));
                case settings::ExportOverlayType::image:
                    return res_type(new ExportImageOverlaySettings(imageOverlay));
                case settings::ExportOverlayType::text:
                    return res_type(new ExportTextOverlaySettings(textOverlay));
                case settings::ExportOverlayType::bookmark:
                    return res_type(new ExportTextOverlaySettings(bookmarkOverlay));
                default:
                    return res_type(nullptr);
            }
        };

    for (const auto type: usedOverlays)
        transcodingSettings.overlays << createRuntimeSettings(type);
}

RapidReviewPersistentSettings::RapidReviewPersistentSettings(bool enabled, int speed) :
    enabled(enabled),
    speed(speed)
{
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(EXPORT_MEDIA_PERSISTENT_TYPES, (json), _Fields)

} // namespace settings
} // namespace desktop
} // namespace client
} // namespace nx

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::client::desktop::settings, ExportOverlayType,
    (nx::client::desktop::settings::ExportOverlayType::timestamp, "timestamp")
    (nx::client::desktop::settings::ExportOverlayType::image, "image")
    (nx::client::desktop::settings::ExportOverlayType::text, "text")
    (nx::client::desktop::settings::ExportOverlayType::bookmark, "bookmark")
)
