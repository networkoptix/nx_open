// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtGui/QFont>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/enum_instrument.h>
#include <nx/vms/client/desktop/common/utils/filesystem.h>
#include <nx/vms/client/desktop/export/data/export_media_settings.h>
#include <nx/utils/serialization/qt_core_types.h>

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

struct NX_VMS_CLIENT_DESKTOP_API ExportTextOverlayPersistentSettings: public ExportTextOverlayPersistentSettingsBase
{
};
#define ExportTextOverlayPersistentSettings_Fields \
    ExportTextOverlayPersistentSettingsBase_Fields (text)

struct NX_VMS_CLIENT_DESKTOP_API ExportBookmarkOverlayPersistentSettings: public ExportTextOverlayPersistentSettingsBase
{
    bool includeDescription = true;

    ExportBookmarkOverlayPersistentSettings();
};
#define ExportBookmarkOverlayPersistentSettings_Fields \
    ExportTextOverlayPersistentSettingsBase_Fields (includeDescription)

struct NX_VMS_CLIENT_DESKTOP_API ExportTimestampOverlayPersistentSettings: public ExportOverlayPersistentSettings
{
    Qt::DateFormat format = Qt::DefaultLocaleLongDate;
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

struct NX_VMS_CLIENT_DESKTOP_API ExportInfoOverlayPersistentSettings: public ExportTextOverlayPersistentSettingsBase
{
    Qt::DateFormat format = Qt::DefaultLocaleShortDate;
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

struct ExportRapidReviewPersistentSettings
{
    bool enabled = false;
    int speed = 1;

    explicit ExportRapidReviewPersistentSettings() = default;
    ExportRapidReviewPersistentSettings(bool enabled, int speed);
};
#define ExportRapidReviewPersistentSettings_Fields (enabled)(speed)

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

QN_FUSION_DECLARE_FUNCTIONS(ExportTimestampOverlayPersistentSettings, (json))
QN_FUSION_DECLARE_FUNCTIONS(ExportImageOverlayPersistentSettings, (json))
QN_FUSION_DECLARE_FUNCTIONS(ExportTextOverlayPersistentSettings, (json))
QN_FUSION_DECLARE_FUNCTIONS(ExportBookmarkOverlayPersistentSettings, (json))
QN_FUSION_DECLARE_FUNCTIONS(ExportInfoOverlayPersistentSettings, (json))
QN_FUSION_DECLARE_FUNCTIONS(ExportRapidReviewPersistentSettings, (json))
QN_FUSION_DECLARE_FUNCTIONS(ExportMediaPersistentSettings, (json))

} // namespace nx::vms::client::desktop

Q_DECLARE_METATYPE(nx::vms::client::desktop::ExportTimestampOverlayPersistentSettings)
Q_DECLARE_METATYPE(nx::vms::client::desktop::ExportImageOverlayPersistentSettings)
Q_DECLARE_METATYPE(nx::vms::client::desktop::ExportTextOverlayPersistentSettings)
Q_DECLARE_METATYPE(nx::vms::client::desktop::ExportBookmarkOverlayPersistentSettings)
Q_DECLARE_METATYPE(nx::vms::client::desktop::ExportInfoOverlayPersistentSettings)
Q_DECLARE_METATYPE(nx::vms::client::desktop::ExportRapidReviewPersistentSettings)
Q_DECLARE_METATYPE(nx::vms::client::desktop::ExportMediaPersistentSettings)
Q_DECLARE_METATYPE(nx::vms::client::desktop::ExportOverlayType)
