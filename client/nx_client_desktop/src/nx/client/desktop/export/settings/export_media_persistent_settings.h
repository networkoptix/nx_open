#pragma once

#include <nx/fusion/model_functions_fwd.h>
#include <nx/client/desktop/export/data/export_media_settings.h>

namespace nx {
namespace client {
namespace desktop {

enum class ExportOverlayType
{
    timestamp,
    image,
    text,
    bookmark,

    none,
    overlayCount = none
};

struct ExportOverlayPersistentSettings
{
    QPoint offset;
    Qt::Alignment alignment = Qt::AlignLeft | Qt::AlignTop;

    virtual nx::core::transcoding::OverlaySettingsPtr createRuntimeSettings() const = 0;
};
#define ExportOverlayPersistentSettings_Fields (offset)(alignment)

struct ExportImageOverlayPersistentSettings: public ExportOverlayPersistentSettings
{
    QImage image;
    QString name;
    int overlayWidth = 320;
    qreal opacity = 1.0;

    ExportImageOverlayPersistentSettings();
    virtual nx::core::transcoding::OverlaySettingsPtr createRuntimeSettings() const override;
};
#define ExportImageOverlayPersistentSettings_Fields \
    ExportOverlayPersistentSettings_Fields \
    (name)(overlayWidth)(opacity)

struct ExportTextOverlayPersistentSettingsBase: public ExportOverlayPersistentSettings
{
    QString text;
    int fontSize = 15;
    int overlayWidth = 320;
    int indent = 12;
    QColor foreground = Qt::white;
    QColor background = QColor(0, 0, 0, 0x7F);
    qreal roundingRadius = 4.0;

    virtual nx::core::transcoding::OverlaySettingsPtr createRuntimeSettings() const override;
};
#define ExportTextOverlayPersistentSettingsBase_Fields \
    ExportOverlayPersistentSettings_Fields \
    (fontSize)(overlayWidth)(indent)

struct ExportTextOverlayPersistentSettings: public ExportTextOverlayPersistentSettingsBase
{
};
#define ExportTextOverlayPersistentSettings_Fields \
    ExportTextOverlayPersistentSettingsBase_Fields (text)

struct ExportBookmarkOverlayPersistentSettings: public ExportTextOverlayPersistentSettingsBase
{
    bool includeDescription = true;

    ExportBookmarkOverlayPersistentSettings();
};
#define ExportBookmarkOverlayPersistentSettings_Fields \
    ExportTextOverlayPersistentSettingsBase_Fields (includeDescription)

struct ExportTimestampOverlayPersistentSettings: public ExportOverlayPersistentSettings
{
    Qt::DateFormat format = Qt::DefaultLocaleLongDate;
    int fontSize = 18;
    qint64 serverTimeDisplayOffsetMs = 0;
    QColor foreground = Qt::white;
    QColor outline = Qt::black;

    virtual nx::core::transcoding::OverlaySettingsPtr createRuntimeSettings() const override;
};
#define ExportTimestampOverlayPersistentSettings_Fields \
    ExportOverlayPersistentSettings_Fields \
    (format)(fontSize)

struct ExportRapidReviewPersistentSettings
{
    bool enabled = false;
    int speed = 1;

    explicit ExportRapidReviewPersistentSettings() = default;
    ExportRapidReviewPersistentSettings(bool enabled, int speed);
};
#define ExportRapidReviewPersistentSettings_Fields (enabled)(speed)

struct ExportMediaPersistentSettings
{
    bool applyFilters = false;

    ExportRapidReviewPersistentSettings rapidReview;

    QVector<ExportOverlayType> usedOverlays;

    ExportImageOverlayPersistentSettings imageOverlay;
    ExportTextOverlayPersistentSettings textOverlay;
    ExportTimestampOverlayPersistentSettings timestampOverlay;
    ExportBookmarkOverlayPersistentSettings bookmarkOverlay;

    ExportMediaPersistentSettings() = default;
    ExportMediaPersistentSettings(const QVector<ExportOverlayType>& used): usedOverlays(used) {}

    ExportOverlayPersistentSettings* overlaySettings(ExportOverlayType type);
    const ExportOverlayPersistentSettings* overlaySettings(ExportOverlayType type) const;

    void updateRuntimeSettings(ExportMediaSettings& runtimeSettings) const;
};
#define ExportMediaPersistentSettings_Fields (applyFilters)(rapidReview)\
    (usedOverlays)(imageOverlay)(timestampOverlay)(textOverlay)(bookmarkOverlay)

#define EXPORT_MEDIA_PERSISTENT_TYPES \
    (ExportTimestampOverlayPersistentSettings) \
    (ExportImageOverlayPersistentSettings) \
    (ExportTextOverlayPersistentSettings) \
    (ExportBookmarkOverlayPersistentSettings) \
    (ExportRapidReviewPersistentSettings) \
    (ExportMediaPersistentSettings)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(EXPORT_MEDIA_PERSISTENT_TYPES, (json))

} // namespace desktop
} // namespace client
} // namespace nx

Q_DECLARE_METATYPE(nx::client::desktop::ExportTimestampOverlayPersistentSettings)
Q_DECLARE_METATYPE(nx::client::desktop::ExportImageOverlayPersistentSettings)
Q_DECLARE_METATYPE(nx::client::desktop::ExportTextOverlayPersistentSettings)
Q_DECLARE_METATYPE(nx::client::desktop::ExportBookmarkOverlayPersistentSettings)
Q_DECLARE_METATYPE(nx::client::desktop::ExportRapidReviewPersistentSettings)
Q_DECLARE_METATYPE(nx::client::desktop::ExportMediaPersistentSettings)

QN_FUSION_DECLARE_FUNCTIONS(nx::client::desktop::ExportOverlayType, (metatype)(lexical))
