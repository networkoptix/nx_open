#pragma once

#include <QtCore/QtGlobal>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/vms/client/desktop/common/utils/filesystem.h>
#include <nx/vms/client/desktop/export/data/export_media_settings.h>

namespace nx::vms::client::desktop {

// TODO: #vkutin Stop using int(ExportOverlayType) as an integer index.
//    Introduce QList<ExportOverlayType> allOverlayTypes() method somewhere.
//    Make ExportOverlayType::none a default-initialized value.
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
    Qt::Alignment alignment {Qt::AlignLeft | Qt::AlignTop};

    virtual ~ExportOverlayPersistentSettings() = default;
    virtual nx::core::transcoding::OverlaySettingsPtr createRuntimeSettings() const = 0;
    virtual void rescale(qreal factor);
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
    virtual void rescale(qreal factor) override;
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

    static constexpr int minimumFontSize() { return 10; }

    virtual nx::core::transcoding::OverlaySettingsPtr createRuntimeSettings() const override;
    virtual void rescale(qreal factor) override;
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

    static constexpr int minimumFontSize() { return 10; }

    virtual nx::core::transcoding::OverlaySettingsPtr createRuntimeSettings() const override;
    virtual void rescale(qreal factor) override;
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
    QString fileFormat;

    bool hasVideo = true;

    int dimension = 1080; //< Smaller dimension of exported resolution.

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

    /**
     * Enable transcoding
     * @returns true if transcoding value was changed.
     */
    bool setTranscoding(bool value);

    /**
     * Check if we use output format, that forces usage of filters.
     * @returns true if usage of filters is forced.
     */
    bool areFiltersForced() const;

    /**
     * Check if we add overlays to exported video.
     */
    bool canExportOverlays() const;

    void updateRuntimeSettings(ExportMediaSettings& runtimeSettings) const;
    void setDimension(int newDimension);
};

#define ExportMediaPersistentSettings_Fields (applyFilters)(fileFormat)(dimension)(rapidReview)\
    (usedOverlays)(imageOverlay)(timestampOverlay)(textOverlay)(bookmarkOverlay)

#define EXPORT_MEDIA_PERSISTENT_TYPES \
    (ExportTimestampOverlayPersistentSettings) \
    (ExportImageOverlayPersistentSettings) \
    (ExportTextOverlayPersistentSettings) \
    (ExportBookmarkOverlayPersistentSettings) \
    (ExportRapidReviewPersistentSettings) \
    (ExportMediaPersistentSettings)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(EXPORT_MEDIA_PERSISTENT_TYPES, (json))

} // namespace nx::vms::client::desktop

Q_DECLARE_METATYPE(nx::vms::client::desktop::ExportTimestampOverlayPersistentSettings)
Q_DECLARE_METATYPE(nx::vms::client::desktop::ExportImageOverlayPersistentSettings)
Q_DECLARE_METATYPE(nx::vms::client::desktop::ExportTextOverlayPersistentSettings)
Q_DECLARE_METATYPE(nx::vms::client::desktop::ExportBookmarkOverlayPersistentSettings)
Q_DECLARE_METATYPE(nx::vms::client::desktop::ExportRapidReviewPersistentSettings)
Q_DECLARE_METATYPE(nx::vms::client::desktop::ExportMediaPersistentSettings)

QN_FUSION_DECLARE_FUNCTIONS(nx::vms::client::desktop::ExportOverlayType, (metatype)(lexical))
