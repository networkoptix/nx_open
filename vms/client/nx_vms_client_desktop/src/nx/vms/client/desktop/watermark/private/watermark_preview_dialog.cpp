// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "watermark_preview_dialog.h"
#include "ui_watermark_preview_dialog.h"
#include "../watermark_edit_settings.h"

#include <array>
#include <chrono>

#include <QtGui/QPainter>
#include <QtCore/QScopedValueRollback>

#include <nx/utils/log/assert.h>

#include <nx/core/watermark/watermark.h>
#include <nx/core/watermark/watermark_images.h>

#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/style/helper.h>

namespace nx::vms::client::desktop {

namespace {

const QString kPreviewUsername = "username";

static constexpr int kOpacitySteps = 20;
static constexpr int kFrequencySteps = 5;
static constexpr std::array<double, kFrequencySteps> kFrequencies { 0.1, 0.2, 0.3, 0.4, 0.5 };

int frequencyToIndex(double value)
{
    for (int index = 0; index < kFrequencySteps; ++index)
    {
        if (value <= kFrequencies[index])
            return index;
    }
    return kFrequencySteps;
}

double indexToFrequency(int index)
{
    return kFrequencies[std::clamp(index, 0, kFrequencySteps - 1)];
}

int opacityToIndex(double value)
{
    return (int)(value * kOpacitySteps);
}

double indexToOpacity(int index)
{
    NX_ASSERT(index >= 1 && index <= kOpacitySteps);
    return index / (double) kOpacitySteps;
}

} // namespace

WatermarkPreviewDialog::WatermarkPreviewDialog(QWidget* parent):
    QnButtonBoxDialog(parent),
    ui(new Ui::WatermarkPreviewDialog),
    m_baseImage(qnSkin->pixmap(":/skin/Illustrations/watermark_preview.jpg"))
{
    ui->setupUi(this);

    ui->opacitySlider->setProperty(style::Properties::kSliderFeatures,
        static_cast<int>(style::SliderFeature::FillingUp));
    ui->frequencySlider->setProperty(style::Properties::kSliderFeatures,
        static_cast<int>(style::SliderFeature::FillingUp));

    ui->frequencySlider->setRange(0, kFrequencySteps - 1);
    ui->opacitySlider->setRange(1, kOpacitySteps);

    connect(ui->opacitySlider, &QSlider::valueChanged,
        this, &WatermarkPreviewDialog::updateDataFromControls);
    connect(ui->frequencySlider, &QSlider::valueChanged,
        this, &WatermarkPreviewDialog::updateDataFromControls);

    m_repaintOperation.setCallback( [this] { drawPreview(); });
    m_repaintOperation.setInterval(std::chrono::milliseconds(60));
}

WatermarkPreviewDialog::~WatermarkPreviewDialog()
{
}

bool WatermarkPreviewDialog::editSettings(api::WatermarkSettings& settings, QWidget* parent)
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
    ui->opacitySlider->setValue(opacityToIndex(m_settings.opacity));
    ui->frequencySlider->setValue(frequencyToIndex(m_settings.frequency));
    m_repaintOperation.requestOperation();
}

void WatermarkPreviewDialog::updateDataFromControls()
{
    if (m_lockUpdate)
        return;

    m_settings.frequency = indexToFrequency(ui->frequencySlider->value());
    m_settings.opacity = indexToOpacity(ui->opacitySlider->value());
    m_repaintOperation.requestOperation();
}

void WatermarkPreviewDialog::drawPreview()
{
    QPixmap image = m_baseImage;
    const auto watermark =
        nx::core::createWatermarkImage({m_settings, kPreviewUsername}, QSize(1920, 1080));

    // Draw watermark in logical coordinates.
    const QRectF dest(
        0.0,
        0.0,
        image.width() / image.devicePixelRatio(),
        image.height() / image.devicePixelRatio());
    const QRectF src(0.0, 0.0, watermark.width(), watermark.height());

    QPainter painter(&image);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.drawPixmap(dest, watermark, src);
    painter.end();

    ui->image->setPixmap(image);
}

bool editWatermarkSettings(api::WatermarkSettings& settings, QWidget* parent)
{
    return WatermarkPreviewDialog::editSettings(settings, parent);
}

} // namespace nx::vms::client::desktop
