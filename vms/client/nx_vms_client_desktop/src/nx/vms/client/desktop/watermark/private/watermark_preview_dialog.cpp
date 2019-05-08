#include "watermark_preview_dialog.h"
#include "ui_watermark_preview_dialog.h"
#include "../watermark_edit_settings.h"

#include <QtGui/QPainter>
#include <QtCore/QScopedValueRollback>

#include <nx/core/watermark/watermark.h>
#include <nx/core/watermark/watermark_images.h>

#include <ui/style/helper.h>

namespace nx::vms::client::desktop {

namespace {
const QString kPreviewUsername = "username";
} // namespace

WatermarkPreviewDialog::WatermarkPreviewDialog(QWidget* parent):
    QnButtonBoxDialog(parent),
    ui(new Ui::WatermarkPreviewDialog),
    m_baseImage(new QPixmap(":/skin/system_settings/watermark_preview.png"))
{
    ui->setupUi(this);

    ui->opacitySlider->setProperty(style::Properties::kSliderFeatures,
        static_cast<int>(style::SliderFeature::FillingUp));
    ui->frequencySlider->setProperty(style::Properties::kSliderFeatures,
        static_cast<int>(style::SliderFeature::FillingUp));

    connect(ui->opacitySlider, &QSlider::valueChanged,
        this, &WatermarkPreviewDialog::updateDataFromControls);
    connect(ui->frequencySlider, &QSlider::valueChanged,
        this, &WatermarkPreviewDialog::updateDataFromControls);
}

WatermarkPreviewDialog::~WatermarkPreviewDialog()
{
}

bool WatermarkPreviewDialog::editSettings(QnWatermarkSettings& settings, QWidget* parent)
{
    QScopedPointer<WatermarkPreviewDialog> dialog(new WatermarkPreviewDialog(parent));
    dialog->m_settings = settings;
    dialog->loadDataToUi();

    bool result = (dialog->exec() == Accepted) && (dialog->m_settings != settings);
    if (result)
        settings = dialog->m_settings;
    return result;
}

void WatermarkPreviewDialog::loadDataToUi()
{
    QScopedValueRollback<bool> lock_guard(m_lockUpdate, true);
    ui->opacitySlider->setValue((int) (m_settings.opacity * ui->opacitySlider->maximum()));
    ui->frequencySlider->setValue((int) (m_settings.frequency * ui->frequencySlider->maximum()));
    drawPreview();
}

void WatermarkPreviewDialog::updateDataFromControls()
{
    if (m_lockUpdate)
        return;

    m_settings.frequency = ui->frequencySlider->value() / (double) ui->frequencySlider->maximum();
    m_settings.opacity = ui->opacitySlider->value() / (double) ui->opacitySlider->maximum();
    drawPreview();
}

void WatermarkPreviewDialog::drawPreview()
{
    const QSize imageSize = m_baseImage->size() * devicePixelRatioF();
    QPixmap image = m_baseImage->scaled(imageSize, Qt::KeepAspectRatio,Qt::SmoothTransformation);
    const auto watermark = core::createWatermarkImage({m_settings, kPreviewUsername}, QSize(1920, 1080));
    QPainter painter(&image);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.drawPixmap(image.rect(), watermark, watermark.rect());
    painter.end();
    image.setDevicePixelRatio(devicePixelRatioF()); //< This should go after drawPixmap(watermark).
    ui->image->setPixmap(image);
}

bool editWatermarkSettings(QnWatermarkSettings& settings, QWidget* parent)
{
    return WatermarkPreviewDialog::editSettings(settings, parent);
}

} // namespace nx::vms::client::desktop
