#include "export_settings_dialog_p.h"

#include <QtCore/QDir>
#include <QtCore/QFileInfo>

#include <core/resource/media_resource.h>
#include <core/resource/camera_resource.h>
#include <camera/camera_thumbnail_manager.h>
#include <camera/single_thumbnail_loader.h>
#include <client/client_settings.h>
#include <ui/common/palette.h>
#include <utils/common/event_processors.h>
#include <utils/common/synctime.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/app_info.h>

#include <nx/fusion/serialization/json_functions.h>

#include <nx/client/desktop/utils/layout_thumbnail_loader.h>

namespace nx {
namespace client {
namespace desktop {

ExportSettingsDialog::Private::Private(bool isBookmark, const QSize& previewSize, QObject* parent):
    base_type(parent),
    m_previewSize(previewSize),
    m_isBookmark(isBookmark)
{
    m_exportLayoutSettings.mode = ExportLayoutSettings::Mode::Export;
}

ExportSettingsDialog::Private::~Private()
{
}

void ExportSettingsDialog::Private::loadSettings()
{
    m_exportMediaSettings = m_isBookmark
        ? qnSettings->exportBookmarkSettings()
        : qnSettings->exportMediaSettings();

    m_exportLayoutSettings = qnSettings->exportLayoutSettings();

    auto lastExportDir = qnSettings->lastExportDir();
    if (lastExportDir.isEmpty())
        lastExportDir = qnSettings->mediaFolder();
    if (lastExportDir.isEmpty())
        lastExportDir = QDir::homePath();

    m_exportMediaSettings.fileName.path = lastExportDir;
    m_exportLayoutSettings.filename.path = lastExportDir;

    updateOverlays();

    const auto used = m_exportMediaSettings.usedOverlays;
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
            if (m_isBookmark)
                qnSettings->setExportBookmarkSettings(m_exportMediaSettings);
            else
                qnSettings->setExportMediaSettings(m_exportMediaSettings);

            qnSettings->setLastExportDir(m_exportMediaSettings.fileName.path);
            break;
        }

        case Mode::Layout:
        {
            qnSettings->setExportLayoutSettings(m_exportLayoutSettings);
            qnSettings->setLastExportDir(m_exportLayoutSettings.filename.path);
            break;
        }
    }
}

void ExportSettingsDialog::Private::setAvailableTranscodingSettings(
    const nx::core::transcoding::Settings& settings)
{
    m_availableTranscodingSettings = settings;
    if (!m_exportMediaSettings.applyFilters)
        return;

    updateTranscodingSettings();
    validateSettings(Mode::Media);
}

void ExportSettingsDialog::Private::updateTranscodingSettings()
{
    // TODO: #vkutin #gdm Separate transcoding settings and overlay settings.
    if (m_exportMediaSettings.applyFilters)
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
    if (m_exportMediaSettings.applyFilters == value)
        return;

    m_exportMediaSettings.applyFilters = value;
    updateTranscodingSettings();
    validateSettings(Mode::Media);
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

    m_fullFrameSize = QnCameraThumbnailManager::sizeHintForCamera(camera, QSize());

    const QPair<qreal, qreal> coefficients(
        qreal(m_previewSize.width()) / m_fullFrameSize.width(),
        qreal(m_previewSize.height()) / m_fullFrameSize.height());

    m_overlayScale = qMin(coefficients.first, coefficients.second);

    for (size_t i = 0; i != overlayCount; ++i)
        m_overlays[i]->setScale(m_overlayScale);

    const auto thumbnailSizeLimit = coefficients.first >= coefficients.second
        ? QSize(m_previewSize.width(), 0)
        : QSize(0, m_previewSize.height());

    m_mediaImageProvider.reset(new QnSingleThumbnailLoader(
        camera,
        m_exportMediaSettings.timePeriod.startTimeMs,
        QnThumbnailRequestData::kDefaultRotation,
        thumbnailSizeLimit));

    m_mediaImageProvider->loadAsync();
    validateSettings(Mode::Media);

    // Set defaults that depend on frame size.
    // TODO: FIXME: #vkutin Are these stored or calculated?
    /*
    m_exportMediaSettings.timestampOverlay.fontSize = m_fullFrameSize.height() / 20;

    m_exportMediaSettings.textOverlay.fontSize = m_fullFrameSize.height() / 30;
    m_exportMediaSettings.textOverlay.overlayWidth = m_fullFrameSize.width() / 4;

    m_exportMediaSettings.bookmarkOverlay.fontSize = m_exportMediaSettings.textOverlay.fontSize;
    m_exportMediaSettings.bookmarkOverlay.overlayWidth =
        m_exportMediaSettings.textOverlay.overlayWidth;
    */
    // TODO: #vkutin #gdm Required?
    //emit exportMediaSettingsChanged(m_exportLayoutSettings);
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

    // TODO: #vkutin #GDM Further init.

    // TODO: #vkutin #gdm Required?
    //validateSettings(Mode::Layout);
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

const settings::RapidReviewPersistentSettings&
    ExportSettingsDialog::Private::storedRapidReviewSettings() const
{
    return m_exportMediaSettings.rapidReview;
}

void ExportSettingsDialog::Private::setStoredRapidReviewSettings(
    const settings::RapidReviewPersistentSettings& value)
{
    m_exportMediaSettings.rapidReview = value;
}

ExportSettingsDialog::Mode ExportSettingsDialog::Private::mode() const
{
    return m_mode;
}

void ExportSettingsDialog::Private::setMode(Mode mode)
{
    m_mode = mode;
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

    // Both media and layout can be exported to binary.
    if (utils::AppInfo::isWin64())
        result << FileExtension::exe64;
    else if (utils::AppInfo::isWin32())
        result << FileExtension::exe86;

    return result;
}

void ExportSettingsDialog::Private::overlayPositionChanged(settings::ExportOverlayType type)
{
    auto overlayWidget = overlay(type);
    NX_EXPECT(overlayWidget);
    if (!overlayWidget || overlayWidget->isHidden())
        return;

    auto settings = m_exportMediaSettings.overlaySettings(type);
    NX_EXPECT(settings);
    if (!settings)
        return;

    // TODO: #vkutin Calculate alignment depending on position in widget's parent.
    settings->position = overlayWidget->pos() / m_overlayScale;
    settings->alignment = Qt::AlignLeft | Qt::AlignTop;
}

const settings::ExportMediaPersistent& ExportSettingsDialog::Private::exportMediaSettings() const
{
    return m_exportMediaSettings;
}

const settings::ExportLayoutPersistent& ExportSettingsDialog::Private::exportLayoutSettings() const
{
    return m_exportLayoutSettings;
}

bool ExportSettingsDialog::Private::isOverlayVisible(settings::ExportOverlayType type) const
{
    const auto overlayWidget = overlay(type);
    return overlayWidget && !overlayWidget->isHidden();
}

void ExportSettingsDialog::Private::selectOverlay(settings::ExportOverlayType type)
{
    auto newSelection = overlay(type);
    if (newSelection == m_selectedOverlay)
        return;

    if (m_selectedOverlay)
        m_selectedOverlay->setBorderVisible(false);

    const auto visibilityChanged = newSelection && newSelection->isHidden();
    m_exportMediaSettings.usedOverlays.removeOne(type);
    m_selectedOverlay = newSelection;

    if (m_selectedOverlay)
    {
        m_exportMediaSettings.usedOverlays.push_back(type);
        m_selectedOverlay->setVisible(true);
        m_selectedOverlay->setBorderVisible(true);
        m_selectedOverlay->raise();
    }

    m_exportMediaSettings.updateRuntimeSettings(); //< TODO: sub-optimal.

    emit overlaySelected(type);

    if (visibilityChanged)
        validateSettings(Mode::Media);
}

void ExportSettingsDialog::Private::hideOverlay(settings::ExportOverlayType type)
{
    auto overlayWidget = overlay(type);
    if (!overlayWidget || overlayWidget->isHidden())
        return;

    overlayWidget->setHidden(true);
    m_exportMediaSettings.usedOverlays.removeOne(type);
    m_exportMediaSettings.updateRuntimeSettings(); //< TODO: sub-optimal.

    validateSettings(Mode::Media);
}

void ExportSettingsDialog::Private::setTimestampOverlaySettings(
    const settings::ExportTimestampOverlayPersistent& settings)
{
    m_exportMediaSettings.timestampOverlay = settings;
    updateOverlay(settings::ExportOverlayType::timestamp);
}

void ExportSettingsDialog::Private::setImageOverlaySettings(
    const settings::ExportImageOverlayPersistent& settings)
{
    m_exportMediaSettings.imageOverlay = settings;
    updateOverlay(settings::ExportOverlayType::image);
}

void ExportSettingsDialog::Private::setTextOverlaySettings(
    const settings::ExportTextOverlayPersistent& settings)
{
    m_exportMediaSettings.textOverlay = settings;
    updateOverlay(settings::ExportOverlayType::text);
}

void ExportSettingsDialog::Private::setBookmarkOverlaySettings(
    const settings::ExportBookmarkOverlayPersistent& settings)
{
    m_exportMediaSettings.bookmarkOverlay = settings;
    // TODO: #vkutin Generate text from bookmark information and passed settings
    updateOverlay(settings::ExportOverlayType::bookmark);
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
        emit validated(m_mode, generateAlerts(results));
    }
    else
    {
        // TODO: #vkutin #gdm
        //const auto results = ???
        //if (m_layoutValidationResults == results)
        //    return;

        //m_layoutValidationResults = results;
        //emit validated(m_mode, generateAlerts(results));
    }
}

void ExportSettingsDialog::Private::updateOverlays()
{
    for (size_t i = 0; i != overlayCount; ++i)
        updateOverlay(static_cast<settings::ExportOverlayType>(i));
}

void ExportSettingsDialog::Private::updateOverlay(settings::ExportOverlayType type)
{
    const auto positionOverlay =
        [this](QWidget* overlay, const ExportOverlaySettings& data)
        {
            // TODO: #vkutin Alignment.
            overlay->move(data.position * m_overlayScale);
        };

    auto overlay = this->overlay(type);
    switch (type)
    {
        case settings::ExportOverlayType::timestamp:
        {
            const auto& data = m_exportMediaSettings.timestampOverlay;
            auto font = overlay->font();
            font.setPixelSize(data.fontSize);
            overlay->setFont(font);

            auto palette = overlay->palette();
            palette.setColor(QPalette::Text, data.foreground);
            palette.setColor(QPalette::Window, Qt::transparent);
            overlay->setPalette(palette);

            updateTimestampText();
            positionOverlay(overlay, data);
            break;
        }

        case settings::ExportOverlayType::image:
        {
            const auto& data = m_exportMediaSettings.imageOverlay;
            overlay->setImage(data.image);
            overlay->setOverlayWidth(data.overlayWidth);
            overlay->setOpacity(data.opacity);
            positionOverlay(overlay, data);
            break;
        }

        case settings::ExportOverlayType::bookmark:
        case settings::ExportOverlayType::text:
        {
            const auto& data = (type == settings::ExportOverlayType::text)
                ? static_cast<const ExportTextOverlaySettings&>(m_exportMediaSettings.textOverlay)
                : m_exportMediaSettings.bookmarkOverlay;

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
            positionOverlay(overlay, data);
            break;
        }

        default:
            NX_ASSERT(false); //< Should not happen.
            break;
    }
}

void ExportSettingsDialog::Private::createOverlays(QWidget* overlayContainer)
{
    if (m_overlays[0])
    {
        NX_ASSERT(false, Q_FUNC_INFO, "ExportSettingsDialog::Private::createOverlays called twice");
        return;
    }

    for (size_t index = 0; index != overlayCount; ++index)
    {
        m_overlays[index] = new ExportOverlayWidget(overlayContainer);
        m_overlays[index]->setHidden(true);

        const auto type = static_cast<settings::ExportOverlayType>(index);

        connect(m_overlays[index], &ExportOverlayWidget::pressed, this,
            [this, type]() { selectOverlay(type); });

        installEventHandler(m_overlays[index], { QEvent::Resize, QEvent::Move, QEvent::Show }, this,
            [this, type]() { overlayPositionChanged(type); });
    }

    setPaletteColor(overlay(settings::ExportOverlayType::image), QPalette::Window, Qt::transparent);

    auto timestampOverlay = overlay(settings::ExportOverlayType::timestamp);
    timestampOverlay->setTextIndent(0);

    static constexpr qreal kShadowBlurRadius = 3.0;
    timestampOverlay->setShadow(Qt::black, QPointF(), kShadowBlurRadius);
}

void ExportSettingsDialog::Private::updateTimestampText()
{
    // TODO: FIXME: #vkutin Change to beginning of selection

    auto currentDayTime = qnSyncTime->currentDateTime();
    currentDayTime = currentDayTime.toOffsetFromUtc(currentDayTime.offsetFromUtc());

    overlay(settings::ExportOverlayType::timestamp)->setText(currentDayTime.toString(
        m_exportMediaSettings.timestampOverlay.format));
}

ExportOverlayWidget* ExportSettingsDialog::Private::overlay(settings::ExportOverlayType type)
{
    const auto index = int(type);
    return index < m_overlays.size() ? m_overlays[index] : nullptr;
}

const ExportOverlayWidget* ExportSettingsDialog::Private::overlay(settings::ExportOverlayType type) const
{
    const auto index = int(type);
    return index < m_overlays.size() ? m_overlays[index] : nullptr;
}

QnSingleThumbnailLoader* ExportSettingsDialog::Private::mediaImageProvider() const
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

QStringList ExportSettingsDialog::Private::generateAlerts(ExportMediaValidator::Results results)
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

                default:
                    NX_EXPECT(false);
                    return QString();
            }
        };

    QStringList alerts;
    for (int i = 0; i < int(ExportMediaValidator::Result::count); ++i)
    {
        if (results.test(i))
            alerts << alertText(ExportMediaValidator::Result(i));
    }

    return alerts;
}

} // namespace desktop
} // namespace client
} // namespace nx
