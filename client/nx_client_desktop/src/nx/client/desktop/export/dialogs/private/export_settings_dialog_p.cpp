#include "export_settings_dialog_p.h"

#include <QtCore/QFileInfo>

#include <ui/style/skin.h>
#include <utils/common/delayed.h>
#include <utils/common/synctime.h>
#include <nx/utils/log/assert.h>

namespace nx {
namespace client {
namespace desktop {
namespace ui {

ExportSettingsDialog::Private::Private(QObject* parent):
    base_type(parent)
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

void ExportSettingsDialog::Private::updateOverlays()
{
    // Text overlay.
    auto textOverlay = overlay(OverlayType::text);
    const auto& textData = m_exportMediaSettings.textOverlay;

    textOverlay->setText(textData.text);
    textOverlay->setTextIndent(textData.indent);
    textOverlay->setOverlayWidth(textData.overlayWidth);
    textOverlay->setRoundingRadius(textData.roundingRadius);

    auto font = textOverlay->font();
    font.setPixelSize(textData.fontSize);
    textOverlay->setFont(font);

    auto palette = textOverlay->palette();
    palette.setColor(QPalette::Text, textData.foreground);
    palette.setColor(QPalette::Window, textData.background);
    textOverlay->setPalette(palette);

    // Image overlay.
    auto imageOverlay = overlay(OverlayType::image);
    const auto& imageData = m_exportMediaSettings.imageOverlay;

    imageOverlay->setImage(imageData.image);
    imageOverlay->setOverlayWidth(imageData.overlayWidth);
    imageOverlay->setOpacity(imageData.opacity);

    palette = imageOverlay->palette();
    palette.setColor(QPalette::Window, imageData.background);
    imageOverlay->setPalette(palette);
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

    overlay(OverlayType::timestamp)->setText(
        qnSyncTime->currentDateTime().toString(Qt::DefaultLocaleLongDate));
}

ExportOverlayWidget* ExportSettingsDialog::Private::overlay(OverlayType type)
{
    return m_overlays[int(type)];
}

const ExportOverlayWidget* ExportSettingsDialog::Private::overlay(OverlayType type) const
{
    return m_overlays[int(type)];
}

} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
