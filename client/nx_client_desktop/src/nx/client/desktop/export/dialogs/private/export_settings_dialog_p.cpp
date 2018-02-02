#include "export_settings_dialog_p.h"

#include <QtCore/QFileInfo>
#include <QtCore/QScopedValueRollback>
#include <QtCore/QStandardPaths>

#include <core/resource/media_resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/camera_bookmark.h>
#include <camera/thumbnails_loader.h>
#include <client/client_settings.h>
#include <nx/core/transcoding/filters/timestamp_filter.h>
#include <ui/common/palette.h>
#include <utils/common/event_processors.h>
#include <utils/common/synctime.h>
#include <utils/common/html.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/app_info.h>
#include <nx/utils/file_system.h>
#include <nx/fusion/model_functions.h>
#include <nx/client/desktop/common/widgets/async_image_widget.h>
#include <nx/client/desktop/image_providers/layout_thumbnail_loader.h>
#include <nx/client/desktop/image_providers/proxy_image_provider.h>
#include <nx/client/desktop/utils/transcoding_image_processor.h>
#include <nx/client/desktop/image_providers/resource_thumbnail_provider.h>

namespace {

static const auto kImageCacheDirName = lit("export_image_overlays");
static const auto kCachedMediaOverlayImageName = lit("export_media_image_overlay.png");
static const auto kCachedBookmarkOverlayImageName = lit("export_bookmark_image_overlay.png");

struct Position
{
    int offset = 0;
    Qt::AlignmentFlag alignment = Qt::AlignmentFlag();

    Position() = default;
    Position(int offset, Qt::AlignmentFlag alignment):
        offset(offset), alignment(alignment)
    {
    }

    bool operator<(const Position& other) const
    {
        return qAbs(offset) < qAbs(other.offset);
    }
};

template<class Settings>
void copyOverlaySettingsWithoutPosition(Settings& dest, Settings src)
{
    // Position is stored in ExportOverlayPersistentSettings base.
    src.nx::client::desktop::ExportOverlayPersistentSettings::operator=(dest); //< Preserve position.
    dest.operator=(src);
}

} // namespace

namespace nx {
namespace client {
namespace desktop {

ExportSettingsDialog::Private::Private(const QnCameraBookmark& bookmark, const QSize& previewSize, QObject* parent):
    base_type(parent),
    m_previewSize(previewSize),
    m_bookmarkName(bookmark.name),
    m_bookmarkDescription(bookmark.description),
    m_mediaPreviewProcessor(new TranscodingImageProcessor())
{
    m_exportLayoutSettings.mode = ExportLayoutSettings::Mode::Export;
}

ExportSettingsDialog::Private::~Private()
{
}

void ExportSettingsDialog::Private::updateOverlaysVisibility(bool transcodingIsAllowed)
{
    if (transcodingIsAllowed)
    {
        for (const auto overlayType: m_exportMediaPersistentSettings.usedOverlays)
            overlay(overlayType)->setHidden(false);
    }
    else
    {
        for (const auto overlayWidget: m_overlays)
            overlayWidget->setHidden(true);
    }
}

void ExportSettingsDialog::Private::loadSettings()
{
    m_exportMediaPersistentSettings = m_bookmarkName.isEmpty()
        ? qnSettings->exportMediaSettings()
        : qnSettings->exportBookmarkSettings();

    m_exportLayoutPersistentSettings = qnSettings->exportLayoutSettings();

    auto lastExportDir = qnSettings->lastExportDir();
    if (lastExportDir.isEmpty())
        lastExportDir = qnSettings->mediaFolder();
    if (lastExportDir.isEmpty())
        lastExportDir = QDir::homePath();

    setMode(QnLexical::deserialized<Mode>(qnSettings->lastExportMode(), Mode::Media));

    m_exportMediaSettings.fileName.path = lastExportDir;
    m_exportMediaSettings.fileName.extension = FileSystemStrings::extension(
        m_exportMediaPersistentSettings.fileFormat, FileExtension::mkv);

    m_exportLayoutSettings.filename.path = lastExportDir;
    m_exportLayoutSettings.filename.extension = FileSystemStrings::extension(
        m_exportLayoutPersistentSettings.fileFormat, FileExtension::nov);

    auto& imageOverlay = m_exportMediaPersistentSettings.imageOverlay;
    if (!imageOverlay.name.trimmed().isEmpty())
    {
        const QImage cachedImage(cachedImageFileName());
        if (cachedImage.isNull())
            imageOverlay.name = QString();
        else
            imageOverlay.image = cachedImage;
    }

    updateOverlays();

    const auto used = m_exportMediaPersistentSettings.usedOverlays;
    for (const auto type: used)
        selectOverlay(type);

    // Should we call it right here? There are no valid resource links here
    refreshMediaPreview();
    updateOverlaysVisibility(isTranscodingRequested());
}

void ExportSettingsDialog::Private::saveSettings()
{
    qnSettings->setLastExportMode(QnLexical::serialized(m_mode));

    switch (m_mode)
    {
        case Mode::Media:
        {
            const auto imageSettings = m_exportMediaPersistentSettings.imageOverlay;
            if (!imageSettings.image.isNull() && !imageSettings.name.trimmed().isEmpty())
                imageSettings.image.save(cachedImageFileName(), "png");

            if (m_bookmarkName.isEmpty())
                qnSettings->setExportMediaSettings(m_exportMediaPersistentSettings);
            else
                qnSettings->setExportBookmarkSettings(m_exportMediaPersistentSettings);

            qnSettings->setLastExportDir(m_exportMediaSettings.fileName.path);
            break;
        }

        case Mode::Layout:
        {
            qnSettings->setExportLayoutSettings(m_exportLayoutPersistentSettings);
            qnSettings->setLastExportDir(m_exportLayoutSettings.filename.path);
            break;
        }
    }
}

void ExportSettingsDialog::Private::setServerTimeZoneOffsetMs(qint64 offsetMs)
{
    m_exportMediaSettings.serverTimeZoneMs = offsetMs;
}

void ExportSettingsDialog::Private::setTimestampOffsetMs(qint64 offsetMs)
{
    m_exportMediaPersistentSettings.timestampOverlay.serverTimeDisplayOffsetMs = offsetMs;
    updateTimestampText();
}

void ExportSettingsDialog::Private::refreshMediaPreview()
{
    if (!m_exportMediaSettings.mediaResource)
    {
        // It is likely we got here from Private::loadSettings.
        // We do not have any valid resource reference here
        return;
    }

    // Fixing settings for preview transcoding
    nx::core::transcoding::Settings& settings = m_exportMediaSettings.transcodingSettings;

    if (m_exportMediaPersistentSettings.applyFilters)
    {
        auto resource = m_exportMediaSettings.mediaResource->toResourcePtr();
        QString forcedRotation = resource->getProperty(QnMediaResource::rotationKey());
        if (!forcedRotation.isEmpty())
            settings.rotation = forcedRotation.toInt();

        settings.aspectRatio = m_availableTranscodingSettings.aspectRatio;
        settings.enhancement = m_availableTranscodingSettings.enhancement;
        settings.dewarping = m_availableTranscodingSettings.dewarping;
        settings.zoomWindow = m_availableTranscodingSettings.zoomWindow;
    }
    else
    {
        settings.rotation = 0;
        settings.aspectRatio = QnAspectRatio();
        settings.enhancement = ImageCorrectionParams();
        settings.dewarping = QnItemDewarpingParams();
        settings.zoomWindow = QRectF();
    }

    m_fullFrameSize = QSize();

    // Requesting base resource image. We will apply transcoding later
    if (m_mediaRawImageProvider == nullptr)
    {
        // Requesting an image without any additional transforms
        // We do have our own image processor, so we do not bother server with transcoding
        api::ResourceImageRequest request;
        request.resource = m_exportMediaSettings.mediaResource->toResourcePtr();
        request.msecSinceEpoch = m_exportMediaSettings.timePeriod.startTimeMs;
        request.roundMethod = api::ImageRequest::RoundMethod::precise;
        request.rotation = 0;
        request.aspectRatio = api::ImageRequest::AspectRatio::source;

        m_mediaRawImageProvider.reset(new ResourceThumbnailProvider(request, this));
    }

    // Initializing a wrapper, that will provide image, processed by m_mediaPreviewProcessor
    if (!m_mediaPreviewProvider)
    {
        std::unique_ptr<ProxyImageProvider> provider(
            new ProxyImageProvider(m_mediaRawImageProvider.get()));
        provider->setImageProcessor(m_mediaPreviewProcessor.data());
        m_mediaPreviewProvider = std::move(provider);
        connect(m_mediaPreviewProvider.get(), &QnImageProvider::sizeHintChanged, this, &Private::setFrameSize);
    }

    m_mediaPreviewProcessor->setTranscodingSettings(settings, m_exportMediaSettings.mediaResource);

    m_mediaPreviewProvider->loadAsync();
    m_mediaPreviewWidget->setImageProvider(m_mediaPreviewProvider.get());
    validateSettings(Mode::Media);
}

void ExportSettingsDialog::Private::setApplyFilters(bool value)
{
    bool transcode = false;
    if (m_exportMediaPersistentSettings.isTranscodingForced())
        transcode = true;
    else
        transcode = value;

    if (m_exportMediaPersistentSettings.setTranscoding(transcode))
    {
        refreshMediaPreview();
    }
}

bool ExportSettingsDialog::Private::isMediaEmpty() const
{
    return m_exportMediaSettings.mediaResource == nullptr || !m_exportMediaSettings.mediaResource->hasVideo(0);
}

void ExportSettingsDialog::Private::setLayoutReadOnly(bool value)
{
    m_exportLayoutPersistentSettings.readOnly = value;
}

void ExportSettingsDialog::Private::setMediaResource(const QnMediaResourcePtr& media, const nx::core::transcoding::Settings& settings)
{
    // We land here once, when ExportSettingsDialog is constructed
    m_availableTranscodingSettings = settings;
    m_exportMediaSettings.mediaResource = media;

    refreshMediaPreview();
}

void ExportSettingsDialog::Private::setFrameSize(const QSize& size)
{
    qDebug() << "ExportSettingsDialog::Private::setFrameSize(" << size << ")";
    if (m_fullFrameSize == size)
        return;

    m_fullFrameSize = size;

    if (m_fullFrameSize.isValid())
    {
        const auto newDimension = std::min(m_fullFrameSize.width(), m_fullFrameSize.height());
        m_exportMediaPersistentSettings.setDimension(newDimension);
    }

    const QPair<qreal, qreal> coefficients(
        qreal(m_previewSize.width()) / m_fullFrameSize.width(),
        qreal(m_previewSize.height()) / m_fullFrameSize.height());

    m_overlayScale = std::min({coefficients.first, coefficients.second, 1.0});

    QScopedValueRollback<bool> updatingRollback(m_positionUpdating, true);

    for (size_t i = 0; i != overlayCount; ++i)
        m_overlays[i]->setScale(m_overlayScale);

    updateOverlays();
    emit frameSizeChanged(size);
}

void ExportSettingsDialog::Private::setLayout(const QnLayoutResourcePtr& layout, const QPalette& palette)
{
    m_exportLayoutSettings.layout = layout;
    m_layoutPreviewProvider.reset();

    if (!layout)
        return;

    std::unique_ptr<LayoutThumbnailLoader> provider( new LayoutThumbnailLoader(
            layout,
            m_previewSize,
            m_exportLayoutSettings.period.startTimeMs));

    provider->setItemBackgroundColor(palette.color(QPalette::Window));
    provider->setFontColor(palette.color(QPalette::WindowText));
    provider->loadAsync();

    m_layoutPreviewProvider = std::move(provider);
    m_layoutPreviewWidget->setImageProvider(m_layoutPreviewProvider.get());
}

void ExportSettingsDialog::Private::setTimePeriod(const QnTimePeriod& period, bool forceValidate)
{
    m_exportMediaSettings.timePeriod = period;
    m_exportLayoutSettings.period = period;
    m_needValidateMedia = true;
    m_needValidateLayout = true;

    if (forceValidate)
    {
        validateSettings(Mode::Media);
        validateSettings(Mode::Layout);
    }
}

void ExportSettingsDialog::Private::setMediaFilename(const Filename& filename)
{
    m_exportMediaSettings.fileName = filename;
    m_exportMediaPersistentSettings.fileFormat = FileSystemStrings::suffix(filename.extension);

    bool needTranscoding =
        isTranscodingRequested() || m_exportMediaPersistentSettings.isTranscodingForced();

    if (m_exportMediaPersistentSettings.setTranscoding(needTranscoding))
    {
        refreshMediaPreview();
    }
    emit transcodingAllowedChanged(needTranscoding);
}

void ExportSettingsDialog::Private::setLayoutFilename(const Filename& filename)
{
    m_exportLayoutSettings.filename = filename;
    m_exportLayoutPersistentSettings.fileFormat = FileSystemStrings::suffix(filename.extension);
    validateSettings(Mode::Layout);
}

void ExportSettingsDialog::Private::setRapidReviewFrameStep(qint64 frameStepMs)
{
    m_exportMediaSettings.timelapseFrameStepMs = frameStepMs;
    validateSettings(Mode::Media);
}

const ExportRapidReviewPersistentSettings&
    ExportSettingsDialog::Private::storedRapidReviewSettings() const
{
    return m_exportMediaPersistentSettings.rapidReview;
}

void ExportSettingsDialog::Private::setStoredRapidReviewSettings(
    const ExportRapidReviewPersistentSettings& value)
{
    m_exportMediaPersistentSettings.rapidReview = value;
}

ExportSettingsDialog::Mode ExportSettingsDialog::Private::mode() const
{
    return m_mode;
}

void ExportSettingsDialog::Private::setMode(Mode mode)
{
    if (m_mode == mode)
        return;

    m_mode = mode;
    validateSettings(m_mode);
}

bool ExportSettingsDialog::Private::isTranscodingRequested() const
{
    return m_exportMediaPersistentSettings.applyFilters;
}

FileExtensionList ExportSettingsDialog::Private::allowedFileExtensions(Mode mode)
{
    FileExtensionList result;
    if (mode == Mode::Media)
        result << FileExtension::mkv << FileExtension::avi << FileExtension::mp4;

    // Both media and layout can be exported to layouts.
    result << FileExtension::nov;
    if (utils::AppInfo::isWin64())
        result << FileExtension::exe64;
    else if (utils::AppInfo::isWin32())
        result << FileExtension::exe86;

    return result;
}

// Computes offset-alignment pair from absolute pixel position
void ExportSettingsDialog::Private::overlayPositionChanged(ExportOverlayType type)
{
    if (!m_fullFrameSize.isValid())
        return;

    auto overlayWidget = overlay(type);
    NX_EXPECT(overlayWidget);
    if (!overlayWidget || overlayWidget->isHidden())
        return;

    auto settings = m_exportMediaPersistentSettings.overlaySettings(type);
    NX_EXPECT(settings);
    if (!settings)
        return;

    const auto geometry = overlayWidget->geometry();
    const auto space = overlayWidget->parentWidget()->size();
    const auto beg = geometry.topLeft();
    const auto end = geometry.bottomRight();
    const auto mid = geometry.center();

    const auto horizontal = std::min({
        Position(beg.x(), Qt::AlignLeft),
        Position(mid.x() - space.width() / 2, Qt::AlignHCenter),
        Position(space.width() - end.x() - 1, Qt::AlignRight)});

    const auto vertical = std::min({
        Position(beg.y(), Qt::AlignTop),
        Position(mid.y() - space.height() / 2, Qt::AlignVCenter),
        Position(space.height() - end.y() - 1, Qt::AlignBottom)});

    settings->offset = QPoint(horizontal.offset, vertical.offset) / m_overlayScale;
    settings->alignment = horizontal.alignment | vertical.alignment;
}

ExportMediaSettings ExportSettingsDialog::Private::exportMediaSettings() const
{
    auto runtimeSettings = m_exportMediaSettings;
    m_exportMediaPersistentSettings.updateRuntimeSettings(runtimeSettings);
    return runtimeSettings;
}

const ExportMediaPersistentSettings&
    ExportSettingsDialog::Private::exportMediaPersistentSettings() const
{
    return m_exportMediaPersistentSettings;
}

ExportLayoutSettings ExportSettingsDialog::Private::exportLayoutSettings() const
{
    auto runtimeSettings = m_exportLayoutSettings;
    m_exportLayoutPersistentSettings.updateRuntimeSettings(runtimeSettings);
    return runtimeSettings;
}

const ExportLayoutPersistentSettings&
    ExportSettingsDialog::Private::exportLayoutPersistentSettings() const
{
    return m_exportLayoutPersistentSettings;
}

bool ExportSettingsDialog::Private::isOverlayVisible(ExportOverlayType type) const
{
    const auto overlayWidget = overlay(type);
    return overlayWidget && !overlayWidget->isHidden();
}

void ExportSettingsDialog::Private::selectOverlay(ExportOverlayType type)
{
    auto newSelection = overlay(type);
    if (newSelection == m_selectedOverlay)
        return;

    if (m_selectedOverlay)
        m_selectedOverlay->setBorderVisible(false);

    const auto visibilityChanged = newSelection && newSelection->isHidden();
    m_exportMediaPersistentSettings.usedOverlays.removeOne(type);
    m_selectedOverlay = newSelection;

    if (m_selectedOverlay)
    {
        m_exportMediaPersistentSettings.usedOverlays.push_back(type);
        m_selectedOverlay->setVisible(true);
        m_selectedOverlay->setBorderVisible(true);
        m_selectedOverlay->raise();
    }

    emit overlaySelected(type);

    if (visibilityChanged)
        validateSettings(Mode::Media);
}

void ExportSettingsDialog::Private::hideOverlay(ExportOverlayType type)
{
    auto overlayWidget = overlay(type);
    if (!overlayWidget || overlayWidget->isHidden())
        return;

    overlayWidget->setHidden(true);
    m_exportMediaPersistentSettings.usedOverlays.removeOne(type);

    validateSettings(Mode::Media);
}

void ExportSettingsDialog::Private::setTimestampOverlaySettings(
    const ExportTimestampOverlayPersistentSettings& settings)
{
    copyOverlaySettingsWithoutPosition(m_exportMediaPersistentSettings.timestampOverlay, settings);
    updateOverlayWidget(ExportOverlayType::timestamp);
}

void ExportSettingsDialog::Private::setImageOverlaySettings(
    const ExportImageOverlayPersistentSettings& settings)
{
    copyOverlaySettingsWithoutPosition(m_exportMediaPersistentSettings.imageOverlay, settings);
    updateOverlayWidget(ExportOverlayType::image);
}

void ExportSettingsDialog::Private::setTextOverlaySettings(
    const ExportTextOverlayPersistentSettings& settings)
{
    copyOverlaySettingsWithoutPosition(m_exportMediaPersistentSettings.textOverlay, settings);
    updateOverlayWidget(ExportOverlayType::text);
}

void ExportSettingsDialog::Private::setBookmarkOverlaySettings(
    const ExportBookmarkOverlayPersistentSettings& settings)
{
    copyOverlaySettingsWithoutPosition(m_exportMediaPersistentSettings.bookmarkOverlay, settings);
    updateBookmarkText();
    updateOverlayWidget(ExportOverlayType::bookmark);
}

void ExportSettingsDialog::Private::validateSettings(Mode mode)
{
    ExportMediaValidator::Results results;
    if (mode == Mode::Media)
    {
        if (!m_exportMediaSettings.mediaResource)
            return;

        m_needValidateMedia = false;
        results = ExportMediaValidator::validateSettings(exportMediaSettings());
        if (m_mediaValidationResults == results)
            return;

        m_mediaValidationResults = results;
    }
    else
    {
        if (!m_exportLayoutSettings.layout)
            return;

        m_needValidateLayout = false;
        results = ExportMediaValidator::validateSettings(exportLayoutSettings());
        if (m_layoutValidationResults == results)
            return;

        m_layoutValidationResults = results;
    }

    QStringList weakAlerts, severeAlerts;
    generateAlerts(results, weakAlerts, severeAlerts);
    emit validated(mode, weakAlerts, severeAlerts);
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
            const auto& data = m_exportMediaPersistentSettings.timestampOverlay;
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

        case ExportOverlayType::image:
        case ExportOverlayType::bookmark:
        case ExportOverlayType::text:
        {
            const auto runtime = m_exportMediaPersistentSettings.overlaySettings(type)
                ->createRuntimeSettings();
            NX_ASSERT(runtime->type() == nx::core::transcoding::OverlaySettings::Type::image);
            if (runtime->type() != nx::core::transcoding::OverlaySettings::Type::image)
                break;
            const auto imageOverlay =
                static_cast<nx::core::transcoding::ImageOverlaySettings*>(runtime.data());
            overlay->setImage(imageOverlay->image);
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
    if (!m_fullFrameSize.isValid())
        return;

    auto overlay = this->overlay(type);
    const auto settings = m_exportMediaPersistentSettings.overlaySettings(type);

    NX_EXPECT(overlay && settings);
    if (!overlay || !settings)
        return;

    QPointF position;
    const auto& space = m_fullFrameSize;
    const auto size = QSizeF(overlay->size()) / m_overlayScale;

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
        qRound(position.x() * m_overlayScale),
        qRound(position.y() * m_overlayScale));
}

void ExportSettingsDialog::Private::createOverlays(QWidget* overlayContainer)
{
    if (m_overlays[0])
    {
        NX_ASSERT(false, Q_FUNC_INFO, "ExportSettingsDialog::Private::createOverlays called twice");
        return;
    }

    installEventHandler(overlayContainer, QEvent::Resize,
        this, &ExportSettingsDialog::Private::updateOverlays);

    for (size_t index = 0; index != overlayCount; ++index)
    {
        m_overlays[index] = new ExportOverlayWidget(overlayContainer);
        m_overlays[index]->setHidden(true);

        const auto type = static_cast<ExportOverlayType>(index);

        connect(m_overlays[index], &ExportOverlayWidget::pressed, this,
            [this, type]() { selectOverlay(type); });

        installEventHandler(m_overlays[index], { QEvent::Resize, QEvent::Move, QEvent::Show }, this,
            [this, type]()
            {
                if (!m_positionUpdating)
                    overlayPositionChanged(type);
            });
    }

    connect(this, &Private::transcodingAllowedChanged, this, &Private::updateOverlaysVisibility);
}

void ExportSettingsDialog::Private::updateBookmarkText()
{
    const auto description = m_exportMediaPersistentSettings.bookmarkOverlay.includeDescription
        ? ensureHtml(m_bookmarkDescription)
        : QString();

    static const auto kBookmarkTemplate = lit("<p><font size=5>%1</font></p>%2");
    const auto text = kBookmarkTemplate.arg(m_bookmarkName.toHtmlEscaped()).arg(description);
    m_exportMediaPersistentSettings.bookmarkOverlay.text = text;
}

QString ExportSettingsDialog::Private::timestampText(qint64 timeMs) const
{
    if (mediaSupportsUtc())
    {
        return nx::core::transcoding::TimestampFilter::timestampTextUtc(
            timeMs,
            m_exportMediaPersistentSettings.timestampOverlay.serverTimeDisplayOffsetMs,
            m_exportMediaPersistentSettings.timestampOverlay.format);
    }

    return nx::core::transcoding::TimestampFilter::timestampTextSimple(timeMs);
}

void ExportSettingsDialog::Private::updateTimestampText()
{
    overlay(ExportOverlayType::timestamp)->setText(timestampText(
        m_exportMediaSettings.timePeriod.startTimeMs));
}

ExportOverlayWidget* ExportSettingsDialog::Private::overlay(ExportOverlayType type)
{
    const auto index = int(type);
    return index < m_overlays.size() ? m_overlays[index] : nullptr;
}

const ExportOverlayWidget* ExportSettingsDialog::Private::overlay(ExportOverlayType type) const
{
    const auto index = int(type);
    return index < m_overlays.size() ? m_overlays[index] : nullptr;
}

void ExportSettingsDialog::Private::setMediaPreviewWidget(nx::client::desktop::AsyncImageWidget* widget)
{
    m_mediaPreviewWidget = widget;
}

void ExportSettingsDialog::Private::setLayoutPreviewWidget(nx::client::desktop::AsyncImageWidget* widget)
{
    m_layoutPreviewWidget = widget;
}

QSize ExportSettingsDialog::Private::fullFrameSize() const
{
    return m_fullFrameSize;
}

Filename ExportSettingsDialog::Private::selectedFileName(Mode mode) const
{
    return mode == Mode::Media
        ? m_exportMediaSettings.fileName
        : m_exportLayoutSettings.filename;
}

bool ExportSettingsDialog::Private::mediaSupportsUtc() const
{
    return m_exportMediaSettings.mediaResource
        && m_exportMediaSettings.mediaResource->toResource()->hasFlags(Qn::utc);
}

void ExportSettingsDialog::Private::generateAlerts(ExportMediaValidator::Results results,
    QStringList& weakAlerts, QStringList& severeAlerts)
{
    const auto alertText =
        [](ExportMediaValidator::Result res) -> QString
        {
            switch (res)
            {
                case ExportMediaValidator::Result::transcoding:
                    return ExportSettingsDialog::tr("Chosen settings require transcoding. "
                        "It will increase CPU usage and may take significant time.");

                case ExportMediaValidator::Result::nonContinuosAvi:
                    return ExportSettingsDialog::tr("AVI format is not recommended to export "
                        "a non-continuous recording with audio track.");

                case ExportMediaValidator::Result::downscaling:
                    return ExportSettingsDialog::tr("We recommend to export video from "
                        "this camera as \"Multi Video\" to avoid downscaling.");

                case ExportMediaValidator::Result::tooLong:
                    return ExportSettingsDialog::tr("You are about to export a long video. "
                        "It may require over a gigabyte of HDD space and take "
                        "several minutes to complete.");

                case ExportMediaValidator::Result::tooBigExeFile:
                    return ExportSettingsDialog::tr("Exported .EXE file will have size over 4 GB "
                        "and cannot be opened by double-click in Windows. "
                        "It can be played only in %1 Client.").arg(QnAppInfo::organizationName());

                case ExportMediaValidator::Result::transcodingInLayoutIsNotSupported:
                    return ExportSettingsDialog::tr("Settings are not available for .NOV and .EXE files.");

                case ExportMediaValidator::Result::nonCameraResources:
                    return ExportSettingsDialog::tr("Local files, server monitor widgets "
                        "and webpages will not be exported.");

                default:
                    NX_EXPECT(false);
                    return QString();
            }
        };

    const auto isSevere =
        [](ExportMediaValidator::Result code)
        {
            return code != ExportMediaValidator::Result::transcoding;
        };

    for (int i = 0; i < int(ExportMediaValidator::Result::count); ++i)
    {
        if (results.test(i))
        {
            const auto code = ExportMediaValidator::Result(i);
            auto& list = isSevere(code) ? severeAlerts : weakAlerts;
            list << alertText(code);
        }
    }
}

QString ExportSettingsDialog::Private::cachedImageFileName() const
{
    const auto baseName = m_bookmarkName.isEmpty()
        ? kCachedMediaOverlayImageName
        : kCachedBookmarkOverlayImageName;

    return imageCacheDir().absoluteFilePath(baseName);
}

QDir ExportSettingsDialog::Private::imageCacheDir()
{
    QDir dir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
        + QDir::separator() + kImageCacheDirName);

    if (!nx::utils::file_system::ensureDir(dir))
        return QDir();

    return dir;
}

} // namespace desktop
} // namespace client
} // namespace nx
