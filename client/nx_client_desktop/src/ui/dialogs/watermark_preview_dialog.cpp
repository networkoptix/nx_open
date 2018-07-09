#include "watermark_preview_dialog.h"
#include "ui_watermark_preview_dialog.h"

#include <QtGui/QPainter>
#include <ui/graphics/items/resource/watermark_painter.h>

namespace {
const QString kPreviewUsername = "username";
} // namespace

QnWatermarkPreviewDialog::QnWatermarkPreviewDialog(QWidget *parent):
    QnButtonBoxDialog(parent),
    ui(new Ui::QnWatermarkPreviewDialog),
    m_painter(new QnWatermarkPainter),
    m_baseImage(new QPixmap(":/skin/system_settings/watermark_preview.png"))
{
    ui->setupUi(this);

    connect(ui->opacitySlider, &QSlider::valueChanged,
        this, &QnWatermarkPreviewDialog::updateDataFromControls);
    connect(ui->frequencySlider, &QSlider::valueChanged,
        this, &QnWatermarkPreviewDialog::updateDataFromControls);
}

QnWatermarkPreviewDialog::~QnWatermarkPreviewDialog()
{
}

bool QnWatermarkPreviewDialog::editSettings(QnWatermarkSettings& settings, QWidget * parent)
{
    QScopedPointer<QnWatermarkPreviewDialog> dialog(new QnWatermarkPreviewDialog(parent));
    dialog->m_settings = settings;
    dialog->loadDataToUi();
    bool result = (dialog->exec() == Accepted) && (dialog->m_settings != settings);
    if(result)
        settings = dialog->m_settings;
    return result;
}

void QnWatermarkPreviewDialog::loadDataToUi()
{
    ui->opacitySlider->setValue((int) (m_settings.opacity * ui->opacitySlider->maximum()));
    ui->frequencySlider->setValue((int) (m_settings.frequency * ui->frequencySlider->maximum()));
    drawPreview();
}

void QnWatermarkPreviewDialog::updateDataFromControls()
{
    m_settings.frequency = ui->frequencySlider->value() / (double) ui->frequencySlider->maximum();
    m_settings.opacity = ui->opacitySlider->value() / (double) ui->opacitySlider->maximum();
    drawPreview();
}

void QnWatermarkPreviewDialog::drawPreview()
{
    QPixmap image = *m_baseImage;
    QPainter painter(&image);

    m_painter->setWatermark(kPreviewUsername, m_settings);
    m_painter->drawWatermark(&painter, image.rect());
    ui->image->setPixmap(image);
}
