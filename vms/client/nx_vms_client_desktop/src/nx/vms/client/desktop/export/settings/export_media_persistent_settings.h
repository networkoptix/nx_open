// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QTimeZone>
#include <QtGui/QFont>

#include <nx/core/transcoding/timestamp_format.h>
#include <nx/reflect/enum_instrument.h>
#include <nx/reflect/instrument.h>
#include <nx/utils/serialization/qt_core_types.h>
#include <nx/utils/serialization/qt_geometry_reflect_json.h>
#include <nx/vms/client/desktop/common/utils/filesystem.h>
#include <nx/vms/client/desktop/export/data/export_media_settings.h>
#include <nx/vms/client/desktop/skin/font_config.h>

class QTextDocument;

namespace nx::vms::client::desktop {

// TODO: #vkutin Stop using int(ExportOverlayType) as an integer index.
//    Introduce QList<ExportOverlayType> allOverlayTypes() method somewhere.
//    Make ExportOverlayType::none a default-initialized value.
enum class ExportOverlayType
{
    timestamp,
    image,
    text,
    info,
    bookmark,

    none,
    overlayCount = none
};

NX_REFLECTION_INSTRUMENT_ENUM(ExportOverlayType, timestamp, image, text, info, bookmark)

struct NX_VMS_CLIENT_DESKTOP_API ExportOverlayPersistentSettings
{
    QPoint offset;
    Qt::Alignment alignment {Qt::AlignLeft | Qt::AlignTop};

    virtual ~ExportOverlayPersistentSettings() = default;
    virtual nx::core::transcoding::OverlaySettingsPtr createRuntimeSettings() const = 0;
    virtual void rescale(qreal factor);

    bool operator==(const ExportOverlayPersistentSettings&) const = default;
};
#define ExportOverlayPersistentSettings_Fields (offset)(alignment)
NX_REFLECTION_INSTRUMENT(ExportOverlayPersistentSettings, ExportOverlayPersistentSettings_Fields)

struct NX_VMS_CLIENT_DESKTOP_API ExportImageOverlayPersistentSettings:
    ExportOverlayPersistentSettings
{
    QImage image;
    QString name;
    int overlayWidth = 320;
    qreal opacity = 1.0;

    ExportImageOverlayPersistentSettings();
    virtual nx::core::transcoding::OverlaySettingsPtr createRuntimeSettings() const override;
    virtual void rescale(qreal factor) override;

    bool operator==(const ExportImageOverlayPersistentSettings& other) const;
};
#define ExportImageOverlayPersistentSettings_Fields \
    ExportOverlayPersistentSettings_Fields \
    (name)(overlayWidth)(opacity)
NX_REFLECTION_INSTRUMENT(ExportImageOverlayPersistentSettings,
    ExportImageOverlayPersistentSettings_Fields)

struct NX_VMS_CLIENT_DESKTOP_API ExportTextOverlayPersistentSettingsBase:
    ExportOverlayPersistentSettings
{
    QString text;
    int fontSize = fontConfig()->large().pixelSize();
    int overlayWidth = 320;
    int indent = 12;
    QColor foreground = Qt::white;
    QColor background = QColor(0, 0, 0, 0x7F);
    qreal roundingRadius = 4.0;

    static constexpr int minimumFontSize() { return 10; }

    virtual nx::core::transcoding::OverlaySettingsPtr createRuntimeSettings() const override;
    virtual void rescale(qreal factor) override;

    bool operator==(const ExportTextOverlayPersistentSettingsBase& other) const;
};
#define ExportTextOverlayPersistentSettingsBase_Fields \
    ExportOverlayPersistentSettings_Fields \
    (fontSize)(overlayWidth)(indent)
NX_REFLECTION_INSTRUMENT(ExportTextOverlayPersistentSettingsBase,
    ExportTextOverlayPersistentSettingsBase_Fields)

struct NX_VMS_CLIENT_DESKTOP_API ExportTextOverlayPersistentSettings: public ExportTextOverlayPersistentSettingsBase
{
};
#define ExportTextOverlayPersistentSettings_Fields \
    ExportTextOverlayPersistentSettingsBase_Fields (text)
NX_REFLECTION_INSTRUMENT(ExportTextOverlayPersistentSettings,
    ExportTextOverlayPersistentSettings_Fields)

struct NX_VMS_CLIENT_DESKTOP_API ExportBookmarkOverlayPersistentSettings:
    ExportTextOverlayPersistentSettingsBase
{
    bool includeDescription = true;

    ExportBookmarkOverlayPersistentSettings();

    bool operator==(const ExportBookmarkOverlayPersistentSettings& other) const = default;
};
#define ExportBookmarkOverlayPersistentSettings_Fields \
    ExportTextOverlayPersistentSettingsBase_Fields (includeDescription)
NX_REFLECTION_INSTRUMENT(ExportBookmarkOverlayPersistentSettings,
    ExportBookmarkOverlayPersistentSettings_Fields)

struct NX_VMS_CLIENT_DESKTOP_API ExportTimestampOverlayPersistentSettings:
    ExportOverlayPersistentSettings
{
    using TimestampFormat = nx::core::transcoding::TimestampFormat;

    TimestampFormat format = TimestampFormat::longDate;
    int fontSize = 18;
    int maximumFontSize = 400;
    QTimeZone timeZone{QTimeZone::LocalTime};
    QColor foreground = Qt::white;
    QColor outline = Qt::black;
    QFont font;

    static constexpr int minimumFontSize() { return 10; }

    virtual nx::core::transcoding::OverlaySettingsPtr createRuntimeSettings() const override;
    virtual void rescale(qreal factor) override;

    bool operator==(const ExportTimestampOverlayPersistentSettings&) const = default;
};
#define ExportTimestampOverlayPersistentSettings_Fields \
    ExportOverlayPersistentSettings_Fields \
    (format)(fontSize)
NX_REFLECTION_INSTRUMENT(ExportTimestampOverlayPersistentSettings,
    ExportTimestampOverlayPersistentSettings_Fields)

struct NX_VMS_CLIENT_DESKTOP_API ExportInfoOverlayPersistentSettings:
    ExportTextOverlayPersistentSettingsBase
{
    using TimestampFormat = nx::core::transcoding::TimestampFormat;

    TimestampFormat format = TimestampFormat::shortDate;
    int maxOverlayWidth = 320;
    int maxOverlayHeight = 240;
    bool exportCameraName = true;
    bool exportDate = false;

    QString cameraNameText;
    QString dateText;

    virtual nx::core::transcoding::OverlaySettingsPtr createRuntimeSettings() const override;
    virtual void rescale(qreal factor) override;

    bool operator==(const ExportInfoOverlayPersistentSettings& other) const = default;

private:
    void fitDocumentHeight(QTextDocument* document, int maxHeight) const;
    QString getElidedText(int maxLength) const;
    QString formatInfoText(const QString& cameraName, const QString& date) const;
};
#define ExportInfoOverlayPersistentSettings_Fields \
    ExportTextOverlayPersistentSettingsBase_Fields (maxOverlayWidth)(maxOverlayHeight)(exportCameraName)(exportDate)
NX_REFLECTION_INSTRUMENT(ExportInfoOverlayPersistentSettings,
    ExportInfoOverlayPersistentSettings_Fields)

struct ExportRapidReviewPersistentSettings
{
    bool enabled = false;
    int speed = 1;

    explicit ExportRapidReviewPersistentSettings() = default;
    ExportRapidReviewPersistentSettings(bool enabled, int speed);

    bool operator==(const ExportRapidReviewPersistentSettings&) const = default;
};
#define ExportRapidReviewPersistentSettings_Fields (enabled)(speed)
NX_REFLECTION_INSTRUMENT(ExportRapidReviewPersistentSettings,
    ExportRapidReviewPersistentSettings_Fields)

struct NX_VMS_CLIENT_DESKTOP_API ExportMediaPersistentSettings
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
    ExportInfoOverlayPersistentSettings infoOverlay;

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

    bool operator==(const ExportMediaPersistentSettings&) const = default;
};

#define ExportMediaPersistentSettings_Fields (applyFilters)(fileFormat)(dimension)(rapidReview)\
    (usedOverlays)(imageOverlay)(timestampOverlay)(textOverlay)(bookmarkOverlay)(infoOverlay)
NX_REFLECTION_INSTRUMENT(ExportMediaPersistentSettings, ExportMediaPersistentSettings_Fields)

} // namespace nx::vms::client::desktop
