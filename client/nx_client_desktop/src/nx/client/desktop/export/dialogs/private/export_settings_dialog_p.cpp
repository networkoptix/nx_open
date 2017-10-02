#include "export_settings_dialog_p.h"

#include <QtCore/QFileInfo>
#include <QtCore/QScopedValueRollback>
#include <QtCore/QStandardPaths>

#include <core/resource/media_resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/camera_bookmark.h>
#include <camera/camera_thumbnail_manager.h>
#include <camera/single_thumbnail_loader.h>
#include <camera/thumbnails_loader.h>
#include <client/client_settings.h>
#include <ui/common/palette.h>
#include <utils/common/event_processors.h>
#include <utils/common/synctime.h>
#include <utils/common/html.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/app_info.h>
#include <nx/utils/file_system.h>
#include <nx/fusion/serialization/json_functions.h>
#include <nx/client/desktop/utils/layout_thumbnail_loader.h>
#include <nx/client/desktop/utils/proxy_image_provider.h>
#include <nx/client/desktop/utils/transcoding_image_processor.h>

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
    m_mediaImageProcessor(new TranscodingImageProcessor())
{
    m_exportLayoutSettings.mode = ExportLayoutSettings::Mode::Export;
}

ExportSettingsDialog::Private::~Private()
{
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

    m_exportMediaSettings.fileName.path = lastExportDir;
    m_exportLayoutSettings.filename.path = lastExportDir;

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

    updateTranscodingSettings();
}

void ExportSettingsDialog::Private::saveSettings()
{
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

void ExportSettingsDialog::Private::setAvailableTranscodingSettings(
    const nx::core::transcoding::Settings& settings)
{
    m_availableTranscodingSettings = settings;
    if (!m_exportMediaPersistentSettings.applyFilters)
        return;

    updateTranscodingSettings();
    validateSettings(Mode::Media);
}

void ExportSettingsDialog::Private::updateTranscodingSettings()
{
    // TODO: #vkutin #gdm Separate transcoding settings and overlay settings.
    if (m_exportMediaPersistentSettings.applyFilters)
    {
        m_exportMediaSettings.transcodingSettings.rotation =
            m_availableTranscodingSettings.rotation;
        m_exportMediaSettings.transcodingSettings.aspectRatio =
            m_availableTranscodingSettings.aspectRatio;
        m_exportMediaSettings.transcodingSettings.enhancement =
            m_availableTranscodingSettings.enhancement;
        m_exportMediaSettings.transcodingSettings.dewarping =
            m_availableTranscodingSettings.dewarping;
        m_exportMediaSettings.transcodingSettings.zoomWindow =
            m_availableTranscodingSettings.zoomWindow;
    }
    else
    {
        m_exportMediaSettings.transcodingSettings.rotation = 0;
        m_exportMediaSettings.transcodingSettings.aspectRatio = QnAspectRatio();
        m_exportMediaSettings.transcodingSettings.enhancement = ImageCorrectionParams();
        m_exportMediaSettings.transcodingSettings.dewarping = QnItemDewarpingParams();
        m_exportMediaSettings.transcodingSettings.zoomWindow = QRectF();
    }

    updateMediaImageProcessor();
}

void ExportSettingsDialog::Private::setApplyFilters(bool value)
{
    if (m_exportMediaPersistentSettings.applyFilters == value)
        return;

    m_exportMediaPersistentSettings.applyFilters = value;
    updateTranscodingSettings();
    validateSettings(Mode::Media);
}

void ExportSettingsDialog::Private::setLayoutReadOnly(bool value)
{
    m_exportLayoutPersistentSettings.readOnly = value;
}

void ExportSettingsDialog::Private::setMediaResource(const QnMediaResourcePtr& media)
{
    m_exportMediaSettings.mediaResource = media;
    m_mediaImageProvider.reset();
    m_fullFrameSize = QSize();

    if (const auto camera = media->toResourcePtr().dynamicCast<QnVirtualCameraResource>())
        setCamera(camera);
    else
        setOtherMedia(media);

    validateSettings(Mode::Media);

    if (!m_mediaImageProvider) //< Just in case.
        return;

    m_mediaImageProvider->setImageProcessor(m_mediaImageProcessor.data());
    m_mediaImageProvider->loadAsync();
}

void ExportSettingsDialog::Private::setCamera(const QnVirtualCameraResourcePtr& camera)
{
    setFrameSize(QnCameraThumbnailManager::sizeHintForCamera(camera, QSize()));

    const QPair<qreal, qreal> coefficients(
        qreal(m_previewSize.width()) / m_fullFrameSize.width(),
        qreal(m_previewSize.height()) / m_fullFrameSize.height());

    m_overlayScale = std::min({ coefficients.first, coefficients.second, 1.0 });

    for (size_t i = 0; i != overlayCount; ++i)
        m_overlays[i]->setScale(m_overlayScale);

    const auto thumbnailSizeLimit = coefficients.first >= coefficients.second
        ? QSize(m_previewSize.width(), 0)
        : QSize(0, m_previewSize.height());

    m_mediaImageProvider.reset(new ProxyImageProvider());

    m_mediaImageProvider->setSourceProvider(new QnSingleThumbnailLoader(
        camera,
        m_exportMediaSettings.timePeriod.startTimeMs,
        0,
        QSize(),
        QnThumbnailRequestData::JpgFormat,
        m_mediaImageProvider.data()));

    connect(m_mediaImageProvider.data(), &QnImageProvider::sizeHintChanged,
        this, &Private::setFrameSize);
}

void ExportSettingsDialog::Private::setOtherMedia(const QnMediaResourcePtr& other)
{
    // TODO: #vkutin #gdm Do not use QnThumbnailsLoader here,
    // implement QnSingleLocalThumbnailLoader as image provider
    // or better support local files in QnSingleThumbnailLoader.

    if (!QnThumbnailsLoader::supportedResource(other))
    {
        NX_ASSERT(false, Q_FUNC_INFO, "Incompatible media resource.");
        return;
    }

    m_mediaImageProvider.reset(new ProxyImageProvider());

    const auto startTimeMs = m_exportMediaSettings.timePeriod.startTimeMs;
    static constexpr auto kSensibleDurationMs = 1000;

    const auto loader = new QnThumbnailsLoader(other);
    loader->setTimePeriod(QnTimePeriod(startTimeMs, kSensibleDurationMs));
    loader->setTimeStep(kSensibleDurationMs);
    loader->setBoundingSize(m_previewSize);

    connect(loader, &QnThumbnailsLoader::thumbnailLoaded, this,
        [this, loader = QPointer<QnThumbnailsLoader>(loader)](const QnThumbnail& thumbnail)
        {
            if (!loader)
                return;

            // At this point we can update frame size.
            setFrameSize(loader->sourceSize());

            // Delete old source provider after setting a new one.
            QScopedPointer<QnImageProvider> oldSource(m_mediaImageProvider->sourceProvider());

            // Set new source provider.
            m_mediaImageProvider->setSourceProvider(new QnBasicImageProvider(
                thumbnail.image(), m_mediaImageProvider.data()));

            // We don't need more than one thumbnail.
            loader->stop();
            loader->disconnect(this);
        });

    loader->start();
}

void ExportSettingsDialog::Private::setFrameSize(const QSize& size)
{
    if (m_fullFrameSize == size)
        return;

    m_fullFrameSize = size;

    const QPair<qreal, qreal> coefficients(
        qreal(m_previewSize.width()) / m_fullFrameSize.width(),
        qreal(m_previewSize.height()) / m_fullFrameSize.height());

    m_overlayScale = qMin(coefficients.first, coefficients.second);

    QScopedValueRollback<bool> updatingRollback(m_positionUpdating, true);

    for (size_t i = 0; i != overlayCount; ++i)
        m_overlays[i]->setScale(m_overlayScale);

    updateOverlays();
    emit frameSizeChanged(size);
}

void ExportSettingsDialog::Private::setLayout(const QnLayoutResourcePtr& layout)
{
    m_exportLayoutSettings.layout = layout;
    m_layoutImageProvider.reset();
    if (!layout)
        return;

    m_layoutImageProvider.reset(new LayoutThumbnailLoader(
        layout, false /*don't allow non-camera resources*/,
        m_previewSize, m_exportLayoutSettings.period.startTimeMs));

    m_layoutImageProvider->loadAsync();
}

void ExportSettingsDialog::Private::setTimePeriod(const QnTimePeriod& period)
{
    m_exportMediaSettings.timePeriod = period;
    m_exportLayoutSettings.period = period;
    validateSettings(Mode::Media);
    validateSettings(Mode::Layout);
}

void ExportSettingsDialog::Private::setMediaFilename(const Filename& filename)
{
    m_exportMediaSettings.fileName = filename;
    validateSettings(Mode::Media);
}

void ExportSettingsDialog::Private::setLayoutFilename(const Filename& filename)
{
    m_exportLayoutSettings.filename = filename;
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

FileExtensionList ExportSettingsDialog::Private::allowedFileExtensions(Mode mode)
{
    FileExtensionList result;
    switch (mode)
    {
        case Mode::Media:
            result
                << FileExtension::mkv
                << FileExtension::avi
                << FileExtension::mp4;
            break;
        case Mode::Layout:
            result << FileExtension::nov;
            break;
        default:
            NX_ASSERT(false, "Should never get here");
            break;
    }

    // TODO: #vkutin Enable binary for media export when it's ready.
    if (mode == Mode::Layout)
    {
        // Both media and layout can be exported to binary.
        if (utils::AppInfo::isWin64())
            result << FileExtension::exe64;
        else if (utils::AppInfo::isWin32())
            result << FileExtension::exe86;
    }

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
    if (mode == Mode::Media)
    {
        if (!m_exportMediaSettings.mediaResource)
            return;

        const auto results = ExportMediaValidator::validateSettings(exportMediaSettings());
        if (m_mediaValidationResults == results)
            return;

        m_mediaValidationResults = results;

        QStringList weakAlerts, severeAlerts;
        generateAlerts(results, weakAlerts, severeAlerts);
        emit validated(Mode::Media, weakAlerts, severeAlerts);
    }
    else
    {
        if (!m_exportLayoutSettings.layout)
            return;

        const auto results = ExportMediaValidator::validateSettings(exportLayoutSettings());
        if (m_layoutValidationResults == results)
            return;

        m_layoutValidationResults = results;

        QStringList weakAlerts, severeAlerts;
        generateAlerts(results, weakAlerts, severeAlerts);
        emit validated(Mode::Layout, weakAlerts, severeAlerts);
    }
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

void ExportSettingsDialog::Private::updateTimestampText()
{
    const auto dateTime = QDateTime::fromMSecsSinceEpoch(m_exportMediaSettings.timePeriod.startTimeMs
        + m_exportMediaPersistentSettings.timestampOverlay.serverTimeDisplayOffsetMs)
        .toOffsetFromUtc(qnSyncTime->currentDateTime().offsetFromUtc());

    overlay(ExportOverlayType::timestamp)->setText(dateTime.toString(
        m_exportMediaPersistentSettings.timestampOverlay.format));
}

void ExportSettingsDialog::Private::updateMediaImageProcessor()
{
    QnLegacyTranscodingSettings processorSettings;
    const auto& settings = m_exportMediaSettings.transcodingSettings;

    processorSettings.itemDewarpingParams = settings.dewarping;
    processorSettings.resource = m_exportMediaSettings.mediaResource;
    processorSettings.contrastParams = settings.enhancement;
    processorSettings.rotation = settings.rotation;
    processorSettings.zoomWindow = settings.zoomWindow;
    processorSettings.forcedAspectRatio = settings.aspectRatio.isValid()
        ? settings.aspectRatio.toFloat()
        : 0.0;

    m_mediaImageProcessor->setTranscodingSettings(processorSettings);
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

QnImageProvider* ExportSettingsDialog::Private::mediaImageProvider() const
{
    return m_mediaImageProvider.data();
}

LayoutThumbnailLoader* ExportSettingsDialog::Private::layoutImageProvider() const
{
    return m_layoutImageProvider.data();
}

QSize ExportSettingsDialog::Private::fullFrameSize() const
{
    return m_fullFrameSize;
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
                        "It can be played only in Nx Witness Client.");

                case ExportMediaValidator::Result::transcodingInBinaryIsNotSupported:
                    return ExportSettingsDialog::tr("Settings are not available for .EXE files.");

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
