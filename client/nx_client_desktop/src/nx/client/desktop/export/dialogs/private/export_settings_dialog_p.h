#pragma once

#include <array>

#include <QtCore/QObject>
#include <QtCore/QPointer>

#include <core/resource/resource_fwd.h>

#include <nx/client/desktop/common/utils/filesystem.h>
#include <nx/client/desktop/export/settings/media_persistent.h>
#include <nx/client/desktop/export/settings/layout_persistent.h>
#include <nx/client/desktop/export/dialogs/export_settings_dialog.h>
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
    enum class ErrorCode
    {
        ok,
    };

    explicit Private(const QSize& previewSize, QObject* parent = nullptr);
    virtual ~Private() override;

    void loadSettings();
    void saveSettings();

    void setMediaResource(const QnMediaResourcePtr& media);
    void setLayout(const QnLayoutResourcePtr& layout);
    void setTimePeriod(const QnTimePeriod& period);
    void setMediaFilename(const Filename& filename);
    void setLayoutFilename(const Filename& filename);
    void setRapidReviewFrameStep(qint64 frameStepMs);

    ErrorCode status() const;
    static bool isExportAllowed(ErrorCode code);

    Mode mode() const;
    void setMode(Mode mode);

    static FileExtensionList allowedFileExtensions(Mode mode);

    const settings::ExportMediaPersistent& exportMediaSettings() const;
    const settings::ExportLayoutPersistent& exportLayoutSettings() const;

    void createOverlays(QWidget* overlayContainer);

    QnSingleThumbnailLoader* mediaImageProvider() const;
    LayoutThumbnailLoader* layoutImageProvider() const;
    QSize fullFrameSize() const;

    // Makes overlay visible and raised and enables its border.
    void selectOverlay(settings::ExportOverlayType type);
    void hideOverlay(settings::ExportOverlayType type);
    bool isOverlayVisible(settings::ExportOverlayType type) const;

    void setTimestampOverlaySettings(const settings::ExportTimestampOverlayPersistent& settings);
    void setImageOverlaySettings(const settings::ExportImageOverlayPersistent& settings);
    void setTextOverlaySettings(const settings::ExportTextOverlayPersistent& settings);
    void setBookmarkOverlaySettings(const settings::ExportBookmarkOverlayPersistent& settings);

    void validateSettings();

signals:
    void statusChanged(ErrorCode value);
    void overlaySelected(settings::ExportOverlayType type);
    void exportMediaSettingsChanged(const ExportMediaSettings&);
    void exportLayoutSettingsChanged(const ExportLayoutSettings&);

private:
    ExportOverlayWidget* overlay(settings::ExportOverlayType type);
    const ExportOverlayWidget* overlay(settings::ExportOverlayType type) const;

    void setStatus(ErrorCode value);
    void updateOverlay(settings::ExportOverlayType type);
    void updateOverlays();
    void updateTimestampText();
    void overlayPositionChanged(settings::ExportOverlayType type);

private:
    const QSize m_previewSize;
    ExportSettingsDialog::Mode m_mode = Mode::Media;
    ErrorCode m_status = ErrorCode::ok;
    settings::ExportMediaPersistent m_exportMediaSettings;
    settings::ExportLayoutPersistent m_exportLayoutSettings;

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
