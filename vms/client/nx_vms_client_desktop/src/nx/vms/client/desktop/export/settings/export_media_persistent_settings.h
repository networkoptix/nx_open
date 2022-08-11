// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtGui/QFont>

#include <nx/core/transcoding/timestamp_format.h>
#include <nx/reflect/enum_instrument.h>
#include <nx/reflect/instrument.h>
#include <nx/utils/serialization/qt_core_types.h>
#include <nx/vms/client/desktop/common/utils/filesystem.h>
#include <nx/vms/client/desktop/export/data/export_media_settings.h>
#include <nx/utils/serialization/qt_geometry_reflect_json.h>

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
};
#define ExportOverlayPersistentSettings_Fields (offset)(alignment)
NX_REFLECTION_INSTRUMENT(ExportOverlayPersistentSettings, ExportOverlayPersistentSettings_Fields)

struct NX_VMS_CLIENT_DESKTOP_API ExportImageOverlayPersistentSettings: public ExportOverlayPersistentSettings
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
NX_REFLECTION_INSTRUMENT(ExportImageOverlayPersistentSettings,
    ExportImageOverlayPersistentSettings_Fields)

struct NX_VMS_CLIENT_DESKTOP_API ExportTextOverlayPersistentSettingsBase: public ExportOverlayPersistentSettings
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
NX_REFLECTION_INSTRUMENT(ExportTextOverlayPersistentSettingsBase,
    ExportTextOverlayPersistentSettingsBase_Fields)

struct NX_VMS_CLIENT_DESKTOP_API ExportTextOverlayPersistentSettings: public ExportTextOverlayPersistentSettingsBase
{
};
#define ExportTextOverlayPersistentSettings_Fields \
    ExportTextOverlayPersistentSettingsBase_Fields (text)
NX_REFLECTION_INSTRUMENT(ExportTextOverlayPersistentSettings,
    ExportTextOverlayPersistentSettings_Fields)

struct NX_VMS_CLIENT_DESKTOP_API ExportBookmarkOverlayPersistentSettings: public ExportTextOverlayPersistentSettingsBase
{
    bool includeDescription = true;

    ExportBookmarkOverlayPersistentSettings();
};
#define ExportBookmarkOverlayPersistentSettings_Fields \
    ExportTextOverlayPersistentSettingsBase_Fields (includeDescription)
NX_REFLECTION_INSTRUMENT(ExportBookmarkOverlayPersistentSettings,
    ExportBookmarkOverlayPersistentSettings_Fields)

struct NX_VMS_CLIENT_DESKTOP_API ExportTimestampOverlayPersistentSettings: public ExportOverlayPersistentSettings
{
    using TimestampFormat = nx::core::transcoding::TimestampFormat;

    TimestampFormat format = TimestampFormat::longDate;
    int fontSize = 18;
    int maximumFontSize = 400;
    qint64 serverTimeDisplayOffsetMs = 0;
    QColor foreground = Qt::white;
    QColor outline = Qt::black;
    QFont font;

    static constexpr int minimumFontSize() { return 10; }

    virtual nx::core::transcoding::OverlaySettingsPtr createRuntimeSettings() const override;
    virtual void rescale(qreal factor) override;
};
#define ExportTimestampOverlayPersistentSettings_Fields \
    ExportOverlayPersistentSettings_Fields \
    (format)(fontSize)
NX_REFLECTION_INSTRUMENT(ExportTimestampOverlayPersistentSettings,
    ExportTimestampOverlayPersistentSettings_Fields)

struct NX_VMS_CLIENT_DESKTOP_API ExportInfoOverlayPersistentSettings: public ExportTextOverlayPersistentSettingsBase
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
};

#define ExportMediaPersistentSettings_Fields (applyFilters)(fileFormat)(dimension)(rapidReview)\
    (usedOverlays)(imageOverlay)(timestampOverlay)(textOverlay)(bookmarkOverlay)(infoOverlay)
NX_REFLECTION_INSTRUMENT(ExportMediaPersistentSettings, ExportMediaPersistentSettings_Fields)

} // namespace nx::vms::client::desktop

Q_DECLARE_METATYPE(nx::vms::client::desktop::ExportTimestampOverlayPersistentSettings)
Q_DECLARE_METATYPE(nx::vms::client::desktop::ExportImageOverlayPersistentSettings)
Q_DECLARE_METATYPE(nx::vms::client::desktop::ExportTextOverlayPersistentSettings)
Q_DECLARE_METATYPE(nx::vms::client::desktop::ExportBookmarkOverlayPersistentSettings)
Q_DECLARE_METATYPE(nx::vms::client::desktop::ExportInfoOverlayPersistentSettings)
Q_DECLARE_METATYPE(nx::vms::client::desktop::ExportRapidReviewPersistentSettings)
Q_DECLARE_METATYPE(nx::vms::client::desktop::ExportMediaPersistentSettings)
Q_DECLARE_METATYPE(nx::vms::client::desktop::ExportOverlayType)
