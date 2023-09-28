// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <array>

#include <QtCore/QDir>
#include <QtCore/QObject>
#include <QtCore/QPointer>

#include <core/resource/resource_fwd.h>
#include <nx/vms/client/desktop/common/flux/flux_state_store.h>
#include <nx/vms/client/desktop/common/utils/filesystem.h>
#include <nx/vms/client/desktop/export/tools/export_media_validator.h>
#include <nx/vms/client/desktop/export/widgets/bookmark_overlay_settings_widget.h>
#include <nx/vms/client/desktop/export/widgets/export_overlay_widget.h>
#include <nx/vms/client/desktop/export/widgets/image_overlay_settings_widget.h>
#include <nx/vms/client/desktop/export/widgets/text_overlay_settings_widget.h>
#include <nx/vms/client/desktop/export/widgets/timestamp_overlay_settings_widget.h>
#include <nx/vms/client/desktop/resource/resource_fwd.h>

#include "../export_settings_dialog.h"
#include "export_settings_dialog_state.h"

class QWidget;
class QnSingleThumbnailLoader;

namespace nx::vms::client::desktop {

class LayoutThumbnailLoader;
class ProxyImageProvider;
class TranscodingImageProcessor;
class ResourceThumbnailProvider;
class AsyncImageWidget;
class ImageProvider;

class ExportSettingsDialog::Private:
    public QObject,
    public FluxStateStore<ExportSettingsDialogState>
{
    Q_OBJECT
    using base_type = QObject;

public:
    explicit Private(const QnCameraBookmark& bookmark,
        const QSize& previewSize,
        const nx::core::Watermark& watermark,
        QObject* parent = nullptr);
    virtual ~Private() override;

    void saveSettings();

    void setMediaResource(const QnMediaResourcePtr& media);
    void setLayout(const LayoutResourcePtr& layout, const QPalette& palette);

    bool mediaSupportsUtc() const;
    Filename selectedFileName(ExportMode mode) const;

    static FileExtensionList allowedFileExtensions(ExportMode mode);

    QnMediaResourcePtr mediaResource() const;
    LayoutResourcePtr layout() const;

    void createOverlays(QWidget* overlayContainer);

    void setMediaPreviewWidget(nx::vms::client::desktop::AsyncImageWidget* widget);
    void setLayoutPreviewWidget(nx::vms::client::desktop::AsyncImageWidget* widget);
    QSize fullFrameSize() const;

    bool isOverlayVisible(ExportOverlayType type) const;

    void validateSettings(ExportMode mode);
    bool hasCameraData() const;

    QString timestampText(qint64 timeMs) const;

    ExportInfoOverlayPersistentSettings getInfoTextData(
        ExportInfoOverlayPersistentSettings data) const;

    // In some cases video is not present, but there can be audio stream
    // We should update overlays and filtering according to this case
    bool hasVideo() const;

    void renderState();

signals:
    void validated(ExportMode mode, const BarDescs&  alertsBySevereLevel);
    void overlaySelected(ExportOverlayType type);
    void frameSizeChanged(const QSize& size);
    void transcodingModeChanged();

private:
    ExportOverlayWidget* overlay(ExportOverlayType type);
    const ExportOverlayWidget* overlay(ExportOverlayType type) const;

    void updateOverlayWidget(ExportOverlayType type);
    void updateOverlayPosition(ExportOverlayType type);
    void updateOverlays();
    void updateTimestampText();
    void updateOverlayImage(
        nx::core::transcoding::OverlaySettingsPtr runtime,
        ExportOverlayWidget* overlay);

    void refreshMediaPreview();
    void updateOverlaysVisibility();

    static BarDescs generateMessageBarDescs(ExportMediaValidator::Results results);
private:
    // Media group

    QnMediaResourcePtr m_mediaResource;
    // Applies filters to make media thumbnails look the same way as on real output
    QScopedPointer<TranscodingImageProcessor> m_mediaPreviewProcessor;
    // Image provider for media preview
    std::unique_ptr<ImageProvider> m_mediaPreviewProvider;
    QPointer<nx::vms::client::desktop::AsyncImageWidget> m_mediaPreviewWidget;

    // This one provides raw image without any transforms
    std::unique_ptr<ResourceThumbnailProvider> m_mediaRawImageProvider;

    // Layout group

    LayoutResourcePtr m_layout;
    // Image provider for layout preview
    std::unique_ptr<ImageProvider> m_layoutPreviewProvider;
    QPointer<nx::vms::client::desktop::AsyncImageWidget> m_layoutPreviewWidget;

    static constexpr size_t overlayCount = size_t(ExportOverlayType::overlayCount);
    std::array<ExportOverlayWidget*, overlayCount> m_overlays {};

    QPointer<ExportOverlayWidget> m_selectedOverlay;

    // Members for tracking partial state updates to reduce state rendering time
    bool m_positionUpdating = false;

    ExportMediaValidator::Results m_mediaValidationResults = 0;
    ExportMediaValidator::Results m_layoutValidationResults = 0;

    bool m_prevNeedTranscoding;
};

} // namespace nx::vms::client::desktop
