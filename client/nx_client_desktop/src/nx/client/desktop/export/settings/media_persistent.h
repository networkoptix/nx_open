#pragma once

#include <nx/fusion/model_functions_fwd.h>
#include <nx/client/desktop/export/data/export_media_settings.h>

namespace nx {
namespace client {
namespace desktop {
namespace settings {

enum class ExportOverlayType
{
    timestamp,
    image,
    text,
    bookmark,

    none,
    overlayCount = none
};

struct ExportImageOverlayPersistent: public ExportImageOverlaySettings
{
    QString name;
    ExportImageOverlayPersistent();
};
#define ExportImageOverlayPersistent_Fields (position)(name)(overlayWidth)(opacity)

struct ExportTextOverlayPersistent: public ExportTextOverlaySettings
{
};
#define ExportTextOverlayPersistent_Fields (position)(text)(fontSize)(overlayWidth)(indent)

struct ExportTimestampOverlayPersistent: public ExportTimestampOverlaySettings
{
};
#define ExportTimestampOverlayPersistent_Fields (position)(alignment)(format)(fontSize)\
    (serverTimeDisplayOffset)

struct ExportBookmarkOverlayPersistent: public ExportTextOverlaySettings
{
    bool includeDescription = true;
    ExportBookmarkOverlayPersistent();
};
#define ExportBookmarkOverlayPersistent_Fields (position)(alignment)(fontSize)(indent)\
    (includeDescription)

struct RapidReviewPersistentSettings
{
    bool enabled = false;
    int speed = 1;

    explicit RapidReviewPersistentSettings() = default;
    RapidReviewPersistentSettings(bool enabled, int speed);
};
#define RapidReviewPersistentSettings_Fields (enabled)(speed)

struct ExportMediaPersistent: public ExportMediaSettings
{
    bool applyFilters = false;

    RapidReviewPersistentSettings rapidReview;

    QVector<ExportOverlayType> usedOverlays;

    ExportImageOverlayPersistent imageOverlay;
    ExportTextOverlayPersistent textOverlay;
    ExportTimestampOverlayPersistent timestampOverlay;
    ExportBookmarkOverlayPersistent bookmarkOverlay;

    ExportOverlaySettings* overlaySettings(ExportOverlayType type);

    // Update low-level settings from hi-level UI persistent settings.
    void updateRuntimeSettings();
};
#define ExportMediaPersistent_Fields (applyFilters)(rapidReview)\
    (usedOverlays)(imageOverlay)(timestampOverlay)(textOverlay)(bookmarkOverlay)

#define EXPORT_MEDIA_PERSISTENT_TYPES (ExportTimestampOverlayPersistent)\
    (ExportImageOverlayPersistent)(ExportTextOverlayPersistent)(ExportBookmarkOverlayPersistent)\
    (RapidReviewPersistentSettings)(ExportMediaPersistent)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(EXPORT_MEDIA_PERSISTENT_TYPES, (json))

} // namespace settings
} // namespace desktop
} // namespace client
} // namespace nx

Q_DECLARE_METATYPE(nx::client::desktop::settings::ExportTimestampOverlayPersistent)
Q_DECLARE_METATYPE(nx::client::desktop::settings::ExportImageOverlayPersistent)
Q_DECLARE_METATYPE(nx::client::desktop::settings::ExportTextOverlayPersistent)
Q_DECLARE_METATYPE(nx::client::desktop::settings::ExportBookmarkOverlayPersistent)
Q_DECLARE_METATYPE(nx::client::desktop::settings::ExportMediaPersistent)

QN_FUSION_DECLARE_FUNCTIONS(nx::client::desktop::settings::ExportOverlayType, (metatype)(lexical))
