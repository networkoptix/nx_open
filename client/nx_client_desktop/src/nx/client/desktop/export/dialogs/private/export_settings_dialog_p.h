#pragma once

#include <array>

#include <QtCore/QObject>
#include <QtCore/QPointer>

#include <core/resource/resource_fwd.h>

#include <nx/client/desktop/common/utils/filesystem.h>
#include <nx/client/desktop/export/dialogs/export_settings_dialog.h>
#include <nx/client/desktop/export/settings/media_persistent.h>
#include <nx/client/desktop/export/settings/layout_persistent.h>
#include <nx/client/desktop/export/tools/export_media_validator.h>
#include <nx/client/desktop/export/widgets/export_overlay_widget.h>
#include <nx/client/desktop/export/widgets/timestamp_overlay_settings_widget.h>
#include <nx/client/desktop/export/widgets/bookmark_overlay_settings_widget.h>
#include <nx/client/desktop/export/widgets/image_overlay_settings_widget.h>
#include <nx/client/desktop/export/widgets/text_overlay_settings_widget.h>

#include <utils/common/connective.h>

class QWidget;
class QnSingleThumbnailLoader;

namespace nx {
namespace client {
namespace desktop {

class LayoutThumbnailLoader;

class ExportSettingsDialog::Private: public Connective<QObject>
{
    Q_OBJECT
    using base_type = Connective<QObject>;

public:
    explicit Private(bool isBookmark, const QSize& previewSize, QObject* parent = nullptr);
    virtual ~Private() override;

    void loadSettings();
    void saveSettings();

    void setMediaResource(const QnMediaResourcePtr& media);
    void setLayout(const QnLayoutResourcePtr& layout);
    void setTimePeriod(const QnTimePeriod& period);
    void setMediaFilename(const Filename& filename);
    void setLayoutFilename(const Filename& filename);
    void setRapidReviewFrameStep(qint64 frameStepMs);
    void setAvailableTranscodingSettings(const nx::core::transcoding::Settings& settings);
    void setApplyFilters(bool value);

    Mode mode() const;
    void setMode(Mode mode);

    static FileExtensionList allowedFileExtensions(Mode mode);

    const settings::ExportMediaPersistent& exportMediaSettings() const;
    const settings::ExportLayoutPersistent& exportLayoutSettings() const;

    void createOverlays(QWidget* overlayContainer);

    QnSingleThumbnailLoader* mediaImageProvider() const;
    LayoutThumbnailLoader* layoutImageProvider() const;
    QSize fullFrameSize() const;

    const settings::RapidReviewPersistentSettings& storedRapidReviewSettings() const;
    void setStoredRapidReviewSettings(const settings::RapidReviewPersistentSettings& value);

    // Makes overlay visible and raised and enables its border.
    void selectOverlay(settings::ExportOverlayType type);
    void hideOverlay(settings::ExportOverlayType type);
    bool isOverlayVisible(settings::ExportOverlayType type) const;

    void setTimestampOverlaySettings(const settings::ExportTimestampOverlayPersistent& settings);
    void setImageOverlaySettings(const settings::ExportImageOverlayPersistent& settings);
    void setTextOverlaySettings(const settings::ExportTextOverlayPersistent& settings);
    void setBookmarkOverlaySettings(const settings::ExportBookmarkOverlayPersistent& settings);

    void validateSettings(Mode mode);

signals:
    void validated(Mode mode, const QStringList& alerts);
    void overlaySelected(settings::ExportOverlayType type);

private:
    ExportOverlayWidget* overlay(settings::ExportOverlayType type);
    const ExportOverlayWidget* overlay(settings::ExportOverlayType type) const;

    void updateOverlay(settings::ExportOverlayType type);
    void updateOverlays();
    void updateTimestampText();
    void overlayPositionChanged(settings::ExportOverlayType type);
    void updateTranscodingSettings();

    static QStringList generateAlerts(ExportMediaValidator::Results results);

private:
    const QSize m_previewSize;
    const bool m_isBookmark = false;
    ExportSettingsDialog::Mode m_mode = Mode::Media;
    ExportMediaValidator::Results m_mediaValidationResults;
    ExportMediaValidator::Results m_layoutValidationResults;
    settings::ExportMediaPersistent m_exportMediaSettings;
    settings::ExportLayoutPersistent m_exportLayoutSettings;
    nx::core::transcoding::Settings m_availableTranscodingSettings;

    static constexpr size_t overlayCount = size_t(settings::ExportOverlayType::overlayCount);
    std::array<ExportOverlayWidget*, overlayCount> m_overlays {};

    QPointer<ExportOverlayWidget> m_selectedOverlay;

    QScopedPointer<QnSingleThumbnailLoader> m_mediaImageProvider;
    QScopedPointer<LayoutThumbnailLoader> m_layoutImageProvider;
    QSize m_fullFrameSize;
};

} // namespace desktop
} // namespace client
} // namespace nx
