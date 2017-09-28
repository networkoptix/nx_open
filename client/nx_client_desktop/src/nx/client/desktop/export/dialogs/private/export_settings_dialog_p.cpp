#include "export_settings_dialog_p.h"

#include <QtCore/QFileInfo>
#include <QtCore/QScopedValueRollback>
#include <QtCore/QStandardPaths>

#include <core/resource/media_resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/camera_bookmark.h>
#include <camera/camera_thumbnail_manager.h>
#include <camera/single_thumbnail_loader.h>
#include <client/client_settings.h>
#include <ui/common/palette.h>
#include <utils/common/event_processors.h>
#include <utils/common/synctime.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/app_info.h>
#include <nx/utils/file_system.h>
#include <nx/fusion/serialization/json_functions.h>
#include <nx/client/desktop/utils/layout_thumbnail_loader.h>
#include <nx/client/desktop/utils/proxy_image_provider.h>

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

} // namespace

namespace nx {
namespace client {
namespace desktop {

ExportSettingsDialog::Private::Private(const QnCameraBookmark& bookmark, const QSize& previewSize, QObject* parent):
    base_type(parent),
    m_previewSize(previewSize),
    m_bookmarkName(bookmark.name),
    m_bookmarkDescription(bookmark.description)
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

void ExportSettingsDialog::Private::setServerTimeOffsetMs(qint64 offsetMs)
{
    m_exportMediaPersistentSettings.timestampOverlay.serverTimeDisplayOffsetMs = offsetMs;
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

    const auto camera = media->toResourcePtr().dynamicCast<QnVirtualCameraResource>();
    if (!camera)
        return;

    // TODO: FIXME: #vkutin #gdm Handle non-camera resources.

    setFrameSize(QnCameraThumbnailManager::sizeHintForCamera(camera, QSize()));

    const QPair<qreal, qreal> coefficients(
        qreal(m_previewSize.width()) / m_fullFrameSize.width(),
        qreal(m_previewSize.height()) / m_fullFrameSize.height());

    m_overlayScale = qMin(coefficients.first, coefficients.second);

    for (size_t i = 0; i != overlayCount; ++i)
        m_overlays[i]->setScale(m_overlayScale);

    const auto thumbnailSizeLimit = coefficients.first >= coefficients.second
        ? QSize(m_previewSize.width(), 0)
        : QSize(0, m_previewSize.height());

    m_mediaImageProvider.reset(new ProxyImageProvider());

    m_mediaImageProvider->setSourceProvider(new QnSingleThumbnailLoader(
        camera,
        m_exportMediaSettings.timePeriod.startTimeMs,
        QnThumbnailRequestData::kDefaultRotation,
        QSize(),
        QnThumbnailRequestData::JpgFormat,
        m_mediaImageProvider.data()));

    connect(m_mediaImageProvider.data(), &QnImageProvider::sizeHintChanged,
        this, &Private::setFrameSize);

    m_mediaImageProvider->loadAsync();
    validateSettings(Mode::Media);
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
}

void ExportSettingsDialog::Private::setLayout(const QnLayoutResourcePtr& layout)
{
    m_exportLayoutSettings.layout = layout;
    m_layoutImageProvider.reset();
    if (!layout)
        return;

    m_layoutImageProvider.reset(new LayoutThumbnailLoader(
        layout, m_previewSize, m_exportLayoutSettings.period.startTimeMs));

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

void ExportSettingsDialog::Private::overlayPositionChanged(ExportOverlayType type)
{
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
    m_exportMediaPersistentSettings.timestampOverlay = settings;
    updateOverlay(ExportOverlayType::timestamp, true);
}

void ExportSettingsDialog::Private::setImageOverlaySettings(
    const ExportImageOverlayPersistentSettings& settings)
{
    m_exportMediaPersistentSettings.imageOverlay = settings;
    updateOverlay(ExportOverlayType::image, true);

    if (settings.image.isNull() || settings.name.trimmed().isEmpty())
        return;

    settings.image.save(cachedImageFileName(), "png");
}

void ExportSettingsDialog::Private::setTextOverlaySettings(
    const ExportTextOverlayPersistentSettings& settings)
{
    m_exportMediaPersistentSettings.textOverlay = settings;
    updateOverlay(ExportOverlayType::text, true);
}

void ExportSettingsDialog::Private::setBookmarkOverlaySettings(
    const ExportBookmarkOverlayPersistentSettings& settings)
{
    m_exportMediaPersistentSettings.bookmarkOverlay = settings;
    updateBookmarkText();
    updateOverlay(ExportOverlayType::bookmark, true);
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
        updateOverlay(static_cast<ExportOverlayType>(i));
}

void ExportSettingsDialog::Private::updateOverlay(ExportOverlayType type,
    bool keepAbsolutePosition)
{
    const auto positionOverlay =
        [this](QWidget* overlay, const ExportOverlayPersistentSettings& data)
        {
            QPointF position;
            const auto& space = m_fullFrameSize;
            const auto size = QSizeF(overlay->size()) / m_overlayScale;

            if (data.alignment.testFlag(Qt::AlignHCenter))
                position.setX((space.width() - size.width()) * 0.5 + data.offset.x());
            else if (data.alignment.testFlag(Qt::AlignRight))
                position.setX(space.width() - size.width() - data.offset.x());
            else
                position.setX(data.offset.x());

            if (data.alignment.testFlag(Qt::AlignVCenter))
                position.setY((space.height() - size.height()) * 0.5 + data.offset.y());
            else if (data.alignment.testFlag(Qt::AlignBottom))
                position.setY(space.height() - size.height() - data.offset.y());
            else
                position.setY(data.offset.y());

            QScopedValueRollback<bool> updatingRollback(m_positionUpdating, true);
            overlay->move(
                qRound(position.x() * m_overlayScale),
                qRound(position.y() * m_overlayScale));
        };

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
            if (!keepAbsolutePosition)
                positionOverlay(overlay, data);
            break;
        }

        case ExportOverlayType::image:
        {
            const auto& data = m_exportMediaPersistentSettings.imageOverlay;
            overlay->setImage(data.image);
            overlay->setOverlayWidth(data.overlayWidth);
            overlay->setOpacity(data.opacity);
            if (!keepAbsolutePosition)
                positionOverlay(overlay, data);
            break;
        }

        case ExportOverlayType::bookmark:
        case ExportOverlayType::text:
        {
            const auto& data = (type == ExportOverlayType::text)
                ? static_cast<const ExportTextOverlayPersistentSettingsBase&>(m_exportMediaPersistentSettings.textOverlay)
                : m_exportMediaPersistentSettings.bookmarkOverlay;

            overlay->setText(data.text);
            overlay->setTextIndent(data.indent);
            overlay->setOverlayWidth(data.overlayWidth);
            overlay->setRoundingRadius(data.roundingRadius);

            auto font = overlay->font();
            font.setPixelSize(data.fontSize);
            overlay->setFont(font);

            auto palette = overlay->palette();
            palette.setColor(QPalette::Text, data.foreground);
            palette.setColor(QPalette::Window, data.background);
            overlay->setPalette(palette);
            if (!keepAbsolutePosition)
                positionOverlay(overlay, data);
            break;
        }

        default:
            NX_ASSERT(false); //< Should not happen.
            break;
    }

    if (keepAbsolutePosition)
        overlayPositionChanged(type);
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

    setPaletteColor(overlay(ExportOverlayType::image), QPalette::Window, Qt::transparent);

    auto timestampOverlay = overlay(ExportOverlayType::timestamp);
    timestampOverlay->setTextIndent(0);

    static constexpr qreal kShadowBlurRadius = 3.0;
    timestampOverlay->setShadow(Qt::black, QPointF(), kShadowBlurRadius);
}

void ExportSettingsDialog::Private::updateBookmarkText()
{
    const auto description = m_exportMediaPersistentSettings.bookmarkOverlay.includeDescription
        ? m_bookmarkDescription
        : QString();

    static const auto kBookmarkTemplate = lit("<p><font size=5>%1</font></p>%2");
    const auto text = kBookmarkTemplate.arg(m_bookmarkName).arg(description);
    m_exportMediaPersistentSettings.bookmarkOverlay.text = text;
    overlay(ExportOverlayType::bookmark)->setText(text);
}

void ExportSettingsDialog::Private::updateTimestampText()
{
    const auto dateTime = QDateTime::fromMSecsSinceEpoch(m_exportMediaSettings.timePeriod.startTimeMs
        + m_exportMediaPersistentSettings.timestampOverlay.serverTimeDisplayOffsetMs)
        .toOffsetFromUtc(qnSyncTime->currentDateTime().offsetFromUtc());

    overlay(ExportOverlayType::timestamp)->setText(dateTime.toString(
        m_exportMediaPersistentSettings.timestampOverlay.format));
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
                    return ExportSettingsDialog::tr("Layout contains non-camera resources. "
                        "They will be exported as \"NO DATA\" placeholders.");

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
