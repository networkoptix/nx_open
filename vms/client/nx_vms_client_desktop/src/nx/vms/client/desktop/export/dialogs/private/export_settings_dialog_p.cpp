// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "export_settings_dialog_p.h"

#include <chrono>

#include <QtCore/QFileInfo>
#include <QtCore/QScopedValueRollback>
#include <QtCore/QStandardPaths>

#include <client_core/client_core_module.h>
#include <common/common_module.h>
#include <core/resource/camera_bookmark.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_resource.h>
#include <nx/build_info.h>
#include <nx/core/transcoding/filters/timestamp_filter.h>
#include <nx/utils/file_system.h>
#include <nx/utils/log/assert.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/common/widgets/async_image_widget.h>
#include <nx/vms/client/desktop/image_providers/layout_thumbnail_loader.h>
#include <nx/vms/client/desktop/image_providers/proxy_image_provider.h>
#include <nx/vms/client/desktop/image_providers/resource_thumbnail_provider.h>
#include <nx/vms/client/desktop/resource/layout_resource.h>
#include <nx/vms/client/desktop/settings/local_settings.h>
#include <nx/vms/client/desktop/settings/message_bar_settings.h>
#include <nx/vms/client/desktop/utils/transcoding_image_processor.h>
#include <ui/common/palette.h>
#include <utils/common/event_processors.h>
#include <utils/common/synctime.h>

namespace nx::vms::client::desktop {

using Reducer = ExportSettingsDialogStateReducer;

ExportSettingsDialog::Private::Private(
    const QnCameraBookmark& bookmark,
    const QSize& previewSize,
    const nx::core::Watermark& watermark,
    QObject* parent)
    :
    base_type(parent),
    FluxStateStore<ExportSettingsDialogState>(previewSize, bookmark.name, bookmark.description),
    m_mediaPreviewProcessor(new TranscodingImageProcessor()),
    m_prevNeedTranscoding(false)
{
    dispatch(Reducer::setWatermark, watermark);
}

ExportSettingsDialog::Private::~Private()
{
}

void ExportSettingsDialog::Private::updateOverlaysVisibility()
{
    if (!state().exportMediaPersistentSettings.canExportOverlays())
    {
        for (const auto overlayWidget: m_overlays)
            overlayWidget->setHidden(true);
        return;
    }

    for (size_t index = 0; index != overlayCount; ++index)
    {
        const ExportOverlayType overlayType = static_cast<ExportOverlayType>(index);
        ExportOverlayWidget* widget = overlay(overlayType);

        const bool isVisible = state().exportMediaPersistentSettings.usedOverlays.contains(overlayType);

        widget->setVisible(isVisible);

        if (!isVisible)
            continue;

        const bool isSelected = overlayType == state().selectedOverlayType;

        widget->setBorderVisible(isSelected);
        if (isSelected)
            widget->raise();
    }
}

// Called outside from ExportSettingsDialog
void ExportSettingsDialog::Private::saveSettings()
{
    appContext()->localSettings()->lastExportMode = state().mode;

    switch (state().mode)
    {
        case ExportMode::media:
        {
            const auto imageSettings = state().exportMediaPersistentSettings.imageOverlay;
            if (!imageSettings.image.isNull() && !imageSettings.name.trimmed().isEmpty())
                imageSettings.image.save(state().cachedImageFileName(), "png");

            if (state().bookmarkName.isEmpty())
            {
                appContext()->localSettings()->exportMediaSettings =
                    state().exportMediaPersistentSettings;
            }
            else
            {
                appContext()->localSettings()->exportBookmarkSettings =
                    state().exportMediaPersistentSettings;
            }

            appContext()->localSettings()->lastExportDir =
                state().exportMediaSettings.fileName.path;
            break;
        }

        case ExportMode::layout:
        {
            appContext()->localSettings()->exportLayoutSettings =
                state().exportLayoutPersistentSettings;
            appContext()->localSettings()->lastExportDir =
                state().exportLayoutSettings.fileName.path;
            break;
        }
    }
}

void ExportSettingsDialog::Private::refreshMediaPreview()
{
    if (m_prevNeedTranscoding == state().exportMediaPersistentSettings.applyFilters)
        return;

    if (!m_mediaResource)
    {
        // It is likely we got here from Private::loadSettings.
        // We do not have any valid resource reference here
        return;
    }

    // Requesting base resource image. We will apply transcoding later.
    if (!m_mediaRawImageProvider)
    {
        // Requesting an image without any additional transforms.
        // We do have our own image processor, so we do not bother server with transcoding.
        // The only server-side applied transform is merging several channels into a single image.
        nx::api::ResourceImageRequest request;
        request.resource = m_mediaResource->toResourcePtr();
        request.timestampMs = std::chrono::milliseconds(
            state().exportMediaSettings.period.startTimeMs);
        request.roundMethod = nx::api::ImageRequest::RoundMethod::iFrameAfter;
        request.rotation = 0;
        request.tolerant = false;
        request.aspectRatio = nx::api::ImageRequest::AspectRatio::source;
        request.streamSelectionMode = api::ThumbnailFilter::StreamSelectionMode::forcedPrimary;

        m_mediaRawImageProvider.reset(new ResourceThumbnailProvider(request, this));
    }

    // Initializing a wrapper, that will provide image, processed by m_mediaPreviewProcessor.
    if (!m_mediaPreviewProvider)
    {
        std::unique_ptr<ProxyImageProvider> provider(
            new ProxyImageProvider(m_mediaRawImageProvider.get()));
        provider->setImageProcessor(m_mediaPreviewProcessor.data());
        m_mediaPreviewProvider = std::move(provider);
        connect(m_mediaPreviewProvider.get(), &ImageProvider::sizeHintChanged, this,
            dispatchTo(Reducer::setFrameSize));
    }

    m_mediaPreviewProcessor->setTranscodingSettings(
        state().exportMediaSettings.transcodingSettings,
        m_mediaResource);

    // We can get here from ExportSettingsDialog::setMediaParams.
    if (!m_mediaResource)
        return;

    m_mediaPreviewProvider->loadAsync();
    m_mediaPreviewWidget->setImageProvider(m_mediaPreviewProvider.get());

    m_prevNeedTranscoding = state().exportMediaPersistentSettings.applyFilters;
}

bool ExportSettingsDialog::Private::hasVideo() const
{
    return m_mediaResource != nullptr && state().exportMediaPersistentSettings.hasVideo;
}

void ExportSettingsDialog::Private::setMediaResource(const QnMediaResourcePtr& media)
{
    m_mediaResource = media;
    m_prevNeedTranscoding = !state().exportMediaPersistentSettings.applyFilters;
}

void ExportSettingsDialog::Private::setLayout(const LayoutResourcePtr& layout, const QPalette& palette)
{
    m_layout = layout;
    m_layoutPreviewProvider.reset();

    if (!layout)
        return;

    std::unique_ptr<LayoutThumbnailLoader> provider( new LayoutThumbnailLoader(
            layout,
            state().previewSize,
            state().exportLayoutSettings.period.startTimeMs));

    provider->setItemBackgroundColor(palette.color(QPalette::Window));
    provider->setFontColor(palette.color(QPalette::WindowText));
    provider->setRequestRoundMethod(nx::api::ResourceImageRequest::RoundMethod::iFrameAfter);
    provider->setTolerant(false);
    provider->setWatermark(state().exportMediaSettings.transcodingSettings.watermark);
    provider->loadAsync();

    m_layoutPreviewProvider = std::move(provider);
    m_layoutPreviewWidget->setImageProvider(m_layoutPreviewProvider.get());
}

FileExtensionList ExportSettingsDialog::Private::allowedFileExtensions(ExportMode mode)
{
    FileExtensionList result;
    if (mode == ExportMode::media)
        result << FileExtension::mkv << FileExtension::avi << FileExtension::mp4;

    // Both media and layout can be exported to layouts.
    result << FileExtension::nov;
    if (nx::build_info::isWindows())
        result << FileExtension::exe;

    return result;
}

QnMediaResourcePtr ExportSettingsDialog::Private::mediaResource() const
{
    return m_mediaResource;
}

LayoutResourcePtr ExportSettingsDialog::Private::layout() const
{
    return m_layout;
}

bool ExportSettingsDialog::Private::isOverlayVisible(ExportOverlayType type) const
{
    const auto overlayWidget = overlay(type);
    return overlayWidget && !overlayWidget->isHidden();
}

void ExportSettingsDialog::Private::validateSettings(ExportMode mode)
{
    ExportMediaValidator::Results results;
    if (mode == ExportMode::media)
    {
        if (!m_mediaResource)
            return;

        results = ExportMediaValidator::validateSettings(state().getExportMediaSettings(), m_mediaResource);
        if (m_mediaValidationResults == results)
            return;

        m_mediaValidationResults = results;
    }
    else
    {
        if (!m_layout)
            return;

        results = ExportMediaValidator::validateSettings(state().getExportLayoutSettings(), m_layout);
        if (m_layoutValidationResults == results)
            return;

        m_layoutValidationResults = results;
    }

    emit validated(mode, generateMessageBarDescs(results));
}

bool ExportSettingsDialog::Private::hasCameraData() const
{
    return !m_mediaValidationResults.test(
        static_cast<int>(ExportMediaValidator::Result::noCameraData));
}

void ExportSettingsDialog::Private::updateOverlays()
{
    for (size_t i = 0; i != overlayCount; ++i)
    {
        const auto overlayType = static_cast<ExportOverlayType>(i);
        updateOverlayWidget(overlayType);
        updateOverlayPosition(overlayType);
    }
}

void ExportSettingsDialog::Private::updateOverlayWidget(ExportOverlayType type)
{
    auto overlay = this->overlay(type);
    switch (type)
    {
        case ExportOverlayType::timestamp:
        {
            const auto& data = state().exportMediaPersistentSettings.timestampOverlay;
            auto font = overlay->font();
            font.setPixelSize(data.fontSize);
            overlay->setFont(font);

            auto palette = overlay->palette();
            palette.setColor(QPalette::Text, data.foreground);
            palette.setColor(QPalette::Window, Qt::transparent);
            overlay->setPalette(palette);

            updateTimestampText();
            break;
        }
        case ExportOverlayType::info:
        {
            if (!m_mediaResource)
                break;

            const auto& data = getInfoTextData(state().exportMediaPersistentSettings.infoOverlay);
            auto font = overlay->font();
            font.setPixelSize(data.fontSize);
            overlay->setFont(font);

            updateOverlayImage(data.createRuntimeSettings(), overlay);
            break;
        }

        case ExportOverlayType::bookmark:
        case ExportOverlayType::image:
        case ExportOverlayType::text:
        {
            const auto runtime = state().exportMediaPersistentSettings.overlaySettings(type)
                ->createRuntimeSettings();
            updateOverlayImage(runtime, overlay);
            break;
        }

        default:
            NX_ASSERT(false); //< Should not happen.
            break;
    }
}

// Computes absolute pixel position by relative offset-alignment pair
void ExportSettingsDialog::Private::updateOverlayPosition(ExportOverlayType type)
{
    auto overlay = this->overlay(type);
    const auto settings = state().exportMediaPersistentSettings.overlaySettings(type);

    NX_ASSERT(overlay && settings);
    if (!overlay || !settings)
        return;

    if (!state().fullFrameSize.isValid())
        return;

    QPointF position;
    const auto& space = state().fullFrameSize;
    const auto size = QSizeF(overlay->size()) / state().overlayScale;

    if (settings->alignment.testFlag(Qt::AlignHCenter))
        position.setX((space.width() - size.width()) * 0.5 + settings->offset.x());
    else if (settings->alignment.testFlag(Qt::AlignRight))
        position.setX(space.width() - size.width() - settings->offset.x());
    else
        position.setX(settings->offset.x());

    if (settings->alignment.testFlag(Qt::AlignVCenter))
        position.setY((space.height() - size.height()) * 0.5 + settings->offset.y());
    else if (settings->alignment.testFlag(Qt::AlignBottom))
        position.setY(space.height() - size.height() - settings->offset.y());
    else
        position.setY(settings->offset.y());

    QScopedValueRollback<bool> updatingRollback(m_positionUpdating, true);
    overlay->move(
        qRound(position.x() * state().overlayScale),
        qRound(position.y() * state().overlayScale));
}

void ExportSettingsDialog::Private::createOverlays(QWidget* overlayContainer)
{
    if (m_overlays[0])
    {
        NX_ASSERT(false, "ExportSettingsDialog::Private::createOverlays called twice");
        return;
    }

    installEventHandler(overlayContainer, QEvent::Resize,
        this, &ExportSettingsDialog::Private::updateOverlays);

    for (size_t index = 0; index != overlayCount; ++index)
    {
        m_overlays[index] = new ExportOverlayWidget(overlayContainer);
        m_overlays[index]->setHidden(true);

        const auto type = static_cast<ExportOverlayType>(index);

        const auto overlayName = QString::fromStdString(nx::reflect::enumeration::toString(type));
        m_overlays[index]->setObjectName(overlayName);

        connect(m_overlays[index], &ExportOverlayWidget::pressed, this,
            [this, type]() { dispatch(Reducer::selectOverlay, type); });

        installEventHandler(m_overlays[index], { QEvent::Resize, QEvent::Move, QEvent::Show }, this,
            [this, type]()
            {
                if (m_positionUpdating)
                    return;

                auto overlayWidget = overlay(type);
                NX_ASSERT(overlayWidget);
                if (!overlayWidget || overlayWidget->isHidden())
                    return;

                const auto geometry = overlayWidget->geometry();
                const auto space = overlayWidget->parentWidget()->size();

                dispatch(Reducer::overlayPositionChanged, type, geometry, space);
            });
    }
}

QString ExportSettingsDialog::Private::timestampText(qint64 timeMs) const
{
    if (mediaSupportsUtc())
    {
        return nx::core::transcoding::TimestampFilter::timestampTextUtc(
            timeMs,
            state().exportMediaPersistentSettings.timestampOverlay.serverTimeDisplayOffsetMs,
            state().exportMediaPersistentSettings.timestampOverlay.format);
    }

    return nx::core::transcoding::TimestampFilter::timestampTextSimple(timeMs);
}

ExportInfoOverlayPersistentSettings ExportSettingsDialog::Private::getInfoTextData(
    ExportInfoOverlayPersistentSettings data) const
{
    if (data.exportCameraName && m_mediaResource)
        data.cameraNameText = m_mediaResource->toResource()->getName();
    else
        data.cameraNameText.clear();

    if (data.exportDate)
    {
        const auto exportTime = nx::utils::millisSinceEpoch();
        const auto exportTimeString = nx::core::transcoding::TimestampFilter::timestampTextUtc(
            exportTime.count(), 0, data.format);

        data.dateText = tr("Exported: %1", "Duration will be substituted").arg(exportTimeString);
    }
    else
    {
        data.dateText.clear();
    }
    return data;
}

void ExportSettingsDialog::Private::updateTimestampText()
{
    overlay(ExportOverlayType::timestamp)->setText(timestampText(
        state().exportMediaSettings.period.startTimeMs));
}

void ExportSettingsDialog::Private::updateOverlayImage(
    nx::core::transcoding::OverlaySettingsPtr runtime,
    ExportOverlayWidget* overlay)
{
    if (!NX_ASSERT(runtime->type() == nx::core::transcoding::OverlaySettings::Type::image))
        return;

    const auto imageOverlay =
        static_cast<nx::core::transcoding::ImageOverlaySettings*>(runtime.data());
    overlay->setImage(imageOverlay->image);
}

ExportOverlayWidget* ExportSettingsDialog::Private::overlay(ExportOverlayType type)
{
    const auto index = int(type);
    return index < (int) m_overlays.size() ? m_overlays[index] : nullptr;
}

const ExportOverlayWidget* ExportSettingsDialog::Private::overlay(ExportOverlayType type) const
{
    const auto index = int(type);
    return index < (int) m_overlays.size() ? m_overlays[index] : nullptr;
}

void ExportSettingsDialog::Private::setMediaPreviewWidget(nx::vms::client::desktop::AsyncImageWidget* widget)
{
    m_mediaPreviewWidget = widget;
}

void ExportSettingsDialog::Private::setLayoutPreviewWidget(nx::vms::client::desktop::AsyncImageWidget* widget)
{
    m_layoutPreviewWidget = widget;
}

QSize ExportSettingsDialog::Private::fullFrameSize() const
{
    return state().fullFrameSize;
}

Filename ExportSettingsDialog::Private::selectedFileName(ExportMode mode) const
{
    return mode == ExportMode::media
        ? state().exportMediaSettings.fileName
        : state().exportLayoutSettings.fileName;
}

bool ExportSettingsDialog::Private::mediaSupportsUtc() const
{
    return m_mediaResource
        && m_mediaResource->toResource()->hasFlags(Qn::utc);
}

ExportSettingsDialog::BarDescs ExportSettingsDialog::Private::generateMessageBarDescs(
    ExportMediaValidator::Results results)
{
    ExportSettingsDialog::BarDescs result;

    const auto barDesc = [](ExportMediaValidator::Result res) -> BarDescription
    {
        switch (res)
        {
            case ExportMediaValidator::Result::transcoding:
                return {.text = ExportSettingsDialog::tr(
                            "Chosen settings require transcoding. "
                            "It will increase CPU usage and may take significant time."),
                    .level = BarDescription::BarLevel::Warning,
                    .isEnabledProperty = &messageBarSettings()->transcodingExportWarning};

            case ExportMediaValidator::Result::aviWithAudio:
                return {.text = ExportSettingsDialog::tr("AVI format is not recommended to export "
                                                         "a recording with audio track."),
                    .level = BarDescription::BarLevel::Info,
                    .isEnabledProperty = &messageBarSettings()->aviWithAudioExportInfo};

            case ExportMediaValidator::Result::downscaling:
                return {.text = ExportSettingsDialog::tr(
                            "We recommend to export video from "
                            "this camera as \"Multi Video\" to avoid downscaling."),
                    .level = BarDescription::BarLevel::Info,
                    .isEnabledProperty = &messageBarSettings()->downscalingExportInfo};

            case ExportMediaValidator::Result::tooLong:
                return {.text = ExportSettingsDialog::tr(
                            "You are about to export a long video. "
                            "It may require a lot of storage space and take significant time."),
                    .level = BarDescription::BarLevel::Warning,
                    .isEnabledProperty = &messageBarSettings()->tooLongExportWarning};

            case ExportMediaValidator::Result::tooBigExeFile:
                return {.text = ExportSettingsDialog::tr(
                            "Exported .EXE file will have size over 4 GB "
                            "and cannot be opened by double-click in Windows. "
                            "It can be played only in %1 Client.")
                                    .arg(nx::branding::company()),
                    .level = BarDescription::BarLevel::Warning,
                    .isEnabledProperty = &messageBarSettings()->tooBigExeFileWarning};

            case ExportMediaValidator::Result::transcodingInLayoutIsNotSupported:
                return {.text = ExportSettingsDialog::tr(
                            "Settings are not available for .NOV and .EXE files."),
                    .level = BarDescription::BarLevel::Info,
                    .isEnabledProperty =
                        &messageBarSettings()->transcodingInLayoutIsNotSupportedWarning};

            case ExportMediaValidator::Result::nonCameraResources:
                return {.text = ExportSettingsDialog::tr("Local files, server monitor widgets "
                                                         "and webpages will not be exported."),
                    .level = BarDescription::BarLevel::Info,
                    .isEnabledProperty = &messageBarSettings()->nonCameraResourcesWarning};

            case ExportMediaValidator::Result::noCameraData:
                return {.text = ExportSettingsDialog::tr(
                            "Export is not available: This camera does not have "
                            "a video archive for the selected time period."),
                    .level = BarDescription::BarLevel::Error};

            case ExportMediaValidator::Result::exportNotAllowed:
                return {
                    .text = ExportSettingsDialog::tr(
                        "You do not have a permission to export "
                        "archive for some of the selected cameras. Video from those cameras will "
                        "not be exported to the resulting file."),
                    .level = BarDescription::BarLevel::Error};

            default:
                NX_ASSERT(false); //<= Unexpected result code.
                return {};
        }
    };

    for (int i = 0; i < int(ExportMediaValidator::Result::count); ++i)
    {
        if (results.test(i))
            result.push_back(barDesc(ExportMediaValidator::Result(i)));
    }

    return result;
}

void ExportSettingsDialog::Private::renderState()
{
    refreshMediaPreview();
    {
        QScopedValueRollback<bool> updatingRollback(m_positionUpdating, true);

        for (size_t i = 0; i != overlayCount; ++i)
            m_overlays[i]->setScale(state().overlayScale);
    }
    updateOverlays();
    updateOverlaysVisibility();

    // Render alerts
    validateSettings(ExportMode::media);
    validateSettings(ExportMode::layout);
}

} // namespace nx::vms::client::desktop
