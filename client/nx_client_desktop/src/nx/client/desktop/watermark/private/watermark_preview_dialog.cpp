#include "../watermark_edit_settings.h"
#include "watermark_preview_dialog.h"
#include "ui_watermark_preview_dialog.h"

#include <QtGui/QPainter>
#include <QtCore/QScopedValueRollback>

#include <nx/client/desktop/watermark/watermark_painter.h>

namespace nx {
namespace client {
namespace desktop {

namespace {
const QString kPreviewUsername = "username";
} // namespace

WatermarkPreviewDialog::WatermarkPreviewDialog(QWidget* parent):
    QnButtonBoxDialog(parent),
    ui(new Ui::WatermarkPreviewDialog),
    m_painter(new WatermarkPainter),
    m_baseImage(new QPixmap(":/skin/system_settings/watermark_preview.png"))
{
    ui->setupUi(this);

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
    QPixmap image = *m_baseImage;
    QPainter painter(&image);

    m_painter->setWatermark(kPreviewUsername, m_settings);
    m_painter->drawWatermark(&painter, image.rect());
    ui->image->setPixmap(image);
}

bool editWatermarkSettings(QnWatermarkSettings& settings, QWidget* parent)
{
    return WatermarkPreviewDialog::editSettings(settings, parent);
}

} // namespace desktop
} // namespace client
} // namespace nx
