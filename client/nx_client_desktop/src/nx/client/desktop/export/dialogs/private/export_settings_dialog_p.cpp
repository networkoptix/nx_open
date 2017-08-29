#include "export_settings_dialog_p.h"

#include <QtCore/QFileInfo>

#include <camera/camera_thumbnail_manager.h>
#include <camera/single_thumbnail_loader.h>
#include <nx/client/desktop/utils/layout_thumbnail_loader.h>
#include <core/resource/media_resource.h>
#include <core/resource/camera_resource.h>
#include <ui/style/skin.h>
#include <utils/common/delayed.h>
#include <utils/common/synctime.h>
#include <nx/utils/log/assert.h>

namespace nx {
namespace client {
namespace desktop {
namespace ui {

ExportSettingsDialog::Private::Private(const QSize& previewSize, QObject* parent):
    base_type(parent),
    m_previewSize(previewSize)
{
}

ExportSettingsDialog::Private::~Private()
{
}

void ExportSettingsDialog::Private::loadSettings()
{
    m_exportMediaSettings.imageOverlay.image =
        qnSkin->pixmap(lit("welcome_page/logo.png")).toImage();

    m_exportMediaSettings.imageOverlay.overlayWidth =
        m_exportMediaSettings.imageOverlay.image.width();

    updateOverlays();
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

    const auto overlayScale = qMin(coefficients.first, coefficients.second);

    for (size_t i = 0; i != overlayCount; ++i)
        m_overlays[i]->setScale(overlayScale);

    const auto thumbnailSizeLimit = coefficients.first >= coefficients.second
        ? QSize(m_previewSize.width(), 0)
        : QSize(0, m_previewSize.height());

    m_mediaImageProvider.reset(new QnSingleThumbnailLoader(
        camera,
        -1, //< Live.
        QnThumbnailRequestData::kDefaultRotation,
        thumbnailSizeLimit));

    m_mediaImageProvider->loadAsync();

    // Set defaults that depend on frame size.
    m_exportMediaSettings.timestampOverlay.fontSize = m_fullFrameSize.height() / 20;
    m_exportMediaSettings.textOverlay.fontSize = m_fullFrameSize.height() / 30;
    m_exportMediaSettings.textOverlay.overlayWidth = m_fullFrameSize.width() / 4;
}

void ExportSettingsDialog::Private::setLayout(const QnLayoutResourcePtr& layout)
{
    m_layoutImageProvider.reset();
    if (!layout)
        return;

    m_layoutImageProvider.reset(new LayoutThumbnailLoader(layout, m_previewSize/*, msecsSinceEpoch*/));
    m_layoutImageProvider->loadAsync();

    // TODO: #vkutin #GDM Further init.
}

void ExportSettingsDialog::Private::setTimePeriod(const QnTimePeriod& period)
{
    m_exportMediaSettings.timePeriod = period;
    m_exportLayoutSettings.period = period;
}

void ExportSettingsDialog::Private::setFilename(const QString& filename)
{
    m_exportMediaSettings.fileName = filename;
    m_exportLayoutSettings.filename = filename;
}

ExportSettingsDialog::Private::ErrorCode ExportSettingsDialog::Private::status() const
{
    return m_status;
}

bool ExportSettingsDialog::Private::isExportAllowed(ErrorCode code)
{
    switch (code)
    {
        case ErrorCode::ok:
            return true;

        default:
            return false;
    }
}

ExportSettingsDialog::Mode ExportSettingsDialog::Private::mode()
{
    return m_mode;
}

const ExportMediaSettings& ExportSettingsDialog::Private::exportMediaSettings() const
{
    return m_exportMediaSettings;
}

const ExportLayoutSettings& ExportSettingsDialog::Private::exportLayoutSettings() const
{
    return m_exportLayoutSettings;
}

void ExportSettingsDialog::Private::setExportMediaSettings(const ExportMediaSettings& settings)
{
    m_exportMediaSettings = settings;
    updateOverlays();
}

void ExportSettingsDialog::Private::setTimestampOverlaySettings(
    const ExportTimestampOverlaySettings& settings)
{
    m_exportMediaSettings.timestampOverlay = settings;
    updateOverlay(OverlayType::timestamp);
}

void ExportSettingsDialog::Private::setImageOverlaySettings(
    const ExportImageOverlaySettings& settings)
{
    m_exportMediaSettings.imageOverlay = settings;
    updateOverlay(OverlayType::image);
}

void ExportSettingsDialog::Private::setTextOverlaySettings(
    const ExportTextOverlaySettings& settings)
{
    m_exportMediaSettings.textOverlay = settings;
    updateOverlay(OverlayType::text);
}

void ExportSettingsDialog::Private::updateOverlays()
{
    for (size_t i = 0; i != overlayCount; ++i)
        updateOverlay(static_cast<OverlayType>(i));
}

void ExportSettingsDialog::Private::updateOverlay(OverlayType type)
{
    auto overlay = this->overlay(type);
    switch (type)
    {
        case OverlayType::timestamp:
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
            break;
        }

        case OverlayType::image:
        {
            const auto& data = m_exportMediaSettings.imageOverlay;
            overlay->setImage(data.image);
            overlay->setOverlayWidth(data.overlayWidth);
            overlay->setOpacity(data.opacity);

            auto palette = overlay->palette();
            palette.setColor(QPalette::Window, data.background);
            overlay->setPalette(palette);
            break;
        }

        case OverlayType::text:
        {
            const auto& data = m_exportMediaSettings.textOverlay;
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
            break;
        }

        default:
            NX_ASSERT(false); //< Should not happen.
            break;
    }
}

void ExportSettingsDialog::Private::setStatus(ErrorCode value)
{
    if (value == m_status)
        return;

    m_status = value;
    emit statusChanged(value);
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
    }

    auto timestampOverlay = overlay(OverlayType::timestamp);
    timestampOverlay->setTextIndent(0);

    static constexpr qreal kShadowBlurRadius = 3.0;
    timestampOverlay->setShadow(Qt::black, QPointF(), kShadowBlurRadius);
}

void ExportSettingsDialog::Private::updateTimestampText()
{
    // TODO: FIXME: #vkutin Change to beginning of selection

    auto currentDayTime = qnSyncTime->currentDateTime();
    currentDayTime = currentDayTime.toOffsetFromUtc(currentDayTime.offsetFromUtc());

    overlay(OverlayType::timestamp)->setText(currentDayTime.toString(
        m_exportMediaSettings.timestampOverlay.format));
}

ExportOverlayWidget* ExportSettingsDialog::Private::overlay(OverlayType type)
{
    return m_overlays[int(type)];
}

const ExportOverlayWidget* ExportSettingsDialog::Private::overlay(OverlayType type) const
{
    return m_overlays[int(type)];
}

QnImageProvider* ExportSettingsDialog::Private::mediaImageProvider() const
{
    return m_mediaImageProvider.data();
}

QnImageProvider* ExportSettingsDialog::Private::layoutImageProvider() const
{
    return m_layoutImageProvider.data();
}

QSize ExportSettingsDialog::Private::fullFrameSize() const
{
    return m_fullFrameSize;
}

} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
