#pragma once

#include <array>

#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QDir>

#include <core/resource/resource_fwd.h>

#include <nx/client/desktop/common/utils/filesystem.h>
#include <nx/client/desktop/export/dialogs/export_settings_dialog.h>
#include <nx/client/desktop/export/settings/export_media_persistent_settings.h>
#include <nx/client/desktop/export/settings/export_layout_persistent_settings.h>
#include <nx/client/desktop/export/tools/export_media_validator.h>
#include <nx/client/desktop/export/widgets/export_overlay_widget.h>
#include <nx/client/desktop/export/widgets/timestamp_overlay_settings_widget.h>
#include <nx/client/desktop/export/widgets/bookmark_overlay_settings_widget.h>
#include <nx/client/desktop/export/widgets/image_overlay_settings_widget.h>
#include <nx/client/desktop/export/widgets/text_overlay_settings_widget.h>

#include <utils/common/connective.h>

class QWidget;
class QnSingleThumbnailLoader;
class QnImageProvider;

namespace nx {
namespace client {
namespace desktop {

class LayoutThumbnailLoader;
class ProxyImageProvider;
class TranscodingImageProcessor;

class ExportSettingsDialog::Private: public Connective<QObject>
{
    Q_OBJECT
    using base_type = Connective<QObject>;

public:
    explicit Private(const QnCameraBookmark& bookmark, const QSize& previewSize,
        QObject* parent = nullptr);
    virtual ~Private() override;

    void loadSettings();
    void saveSettings();

    void setMediaResource(const QnMediaResourcePtr& media);
    void setLayout(const QnLayoutResourcePtr& layout);
    void setTimePeriod(const QnTimePeriod& period);
    void setMediaFilename(const Filename& filename);
    void setLayoutFilename(const Filename& filename);
    void setRapidReviewFrameStep(qint64 frameStepMs);
    void setServerTimeOffsetMs(qint64 offsetMs);
    void setAvailableTranscodingSettings(const nx::core::transcoding::Settings& settings);
    void setApplyFilters(bool value);
    void setLayoutReadOnly(bool value);

    Mode mode() const;
    void setMode(Mode mode);

    static FileExtensionList allowedFileExtensions(Mode mode);

    ExportMediaSettings exportMediaSettings() const;
    const ExportMediaPersistentSettings& exportMediaPersistentSettings() const;

    ExportLayoutSettings exportLayoutSettings() const;
    const ExportLayoutPersistentSettings& exportLayoutPersistentSettings() const;

    void createOverlays(QWidget* overlayContainer);

    QnImageProvider* mediaImageProvider() const;
    LayoutThumbnailLoader* layoutImageProvider() const;
    QSize fullFrameSize() const;

    const ExportRapidReviewPersistentSettings& storedRapidReviewSettings() const;
    void setStoredRapidReviewSettings(const ExportRapidReviewPersistentSettings& value);

    // Makes overlay visible and raised and enables its border.
    void selectOverlay(ExportOverlayType type);
    void hideOverlay(ExportOverlayType type);
    bool isOverlayVisible(ExportOverlayType type) const;

    void setTimestampOverlaySettings(const ExportTimestampOverlayPersistentSettings& settings);
    void setImageOverlaySettings(const ExportImageOverlayPersistentSettings& settings);
    void setTextOverlaySettings(const ExportTextOverlayPersistentSettings& settings);
    void setBookmarkOverlaySettings(const ExportBookmarkOverlayPersistentSettings& settings);

    void validateSettings(Mode mode);

signals:
    void validated(Mode mode, const QStringList& weakAlerts, const QStringList& severeAlerts);
    void overlaySelected(ExportOverlayType type);

private:
    ExportOverlayWidget* overlay(ExportOverlayType type);
    const ExportOverlayWidget* overlay(ExportOverlayType type) const;

    void updateOverlayWidget(ExportOverlayType type);
    void updateOverlayPosition(ExportOverlayType type);
    void updateOverlays();
    void updateBookmarkText();
    void updateTimestampText();
    void overlayPositionChanged(ExportOverlayType type);
    void updateTranscodingSettings();
    void updateMediaImageProcessor();
    QString cachedImageFileName() const;

    void setFrameSize(const QSize& size);

    static void generateAlerts(ExportMediaValidator::Results results,
        QStringList& weakAlerts, QStringList& severeAlerts);

    static QDir imageCacheDir();

private:
    const QSize m_previewSize;
    const QString m_bookmarkName;
    const QString m_bookmarkDescription;
    ExportSettingsDialog::Mode m_mode = Mode::Media;

    ExportMediaValidator::Results m_mediaValidationResults;
    ExportMediaValidator::Results m_layoutValidationResults;

    ExportMediaSettings m_exportMediaSettings;
    ExportMediaPersistentSettings m_exportMediaPersistentSettings;

    ExportLayoutSettings m_exportLayoutSettings;
    ExportLayoutPersistentSettings m_exportLayoutPersistentSettings;

    nx::core::transcoding::Settings m_availableTranscodingSettings;

    static constexpr size_t overlayCount = size_t(ExportOverlayType::overlayCount);
    std::array<ExportOverlayWidget*, overlayCount> m_overlays {};

    QPointer<ExportOverlayWidget> m_selectedOverlay;

    QScopedPointer<ProxyImageProvider> m_mediaImageProvider;
    QScopedPointer<LayoutThumbnailLoader> m_layoutImageProvider;
    QScopedPointer<TranscodingImageProcessor> m_mediaImageProcessor;
    QSize m_fullFrameSize;
    qreal m_overlayScale = 1.0;


    bool m_positionUpdating = false;
};

} // namespace desktop
} // namespace client
} // namespace nx
