#include "image_overlay_settings_widget.h"
#include "ui_image_overlay_settings_widget.h"

#include <QtWidgets/QApplication>

#include <ui/common/aligner.h>

namespace nx {
namespace client {
namespace desktop {
namespace ui {

ImageOverlaySettingsWidget::ImageOverlaySettingsWidget(QWidget* parent):
    base_type(parent),
    ui(new Ui::ImageOverlaySettingsWidget())
{
    ui->setupUi(this);

    auto aligner = new QnAligner(this);
    aligner->addWidgets({ ui->sizeLabel, ui->opacityLabel });

    static const auto kDefaultMaximumWidth = 1000;
    const auto minimumWidth = qMax(QApplication::globalStrut().width(), 1);
    ui->sizeSlider->setRange(minimumWidth, kDefaultMaximumWidth);
    ui->opacitySlider->setRange(0, 100);
    updateControls();

    connect(ui->sizeSlider, &QSlider::valueChanged,
        [this](int value)
        {
            m_data.overlayWidth = value;
            emit dataChanged(m_data);
        });

    connect(ui->opacitySlider, &QSlider::valueChanged,
        [this](int value)
        {
            m_data.opacity = value / 100.0;
            emit dataChanged(m_data);
        });
}

ImageOverlaySettingsWidget::~ImageOverlaySettingsWidget()
{
}

void ImageOverlaySettingsWidget::updateControls()
{
    ui->sizeSlider->setValue(m_data.overlayWidth);
    ui->opacitySlider->setValue(int(m_data.opacity * 100.0));
}

const ExportImageOverlaySettings& ImageOverlaySettingsWidget::data() const
{
    return m_data;
}

void ImageOverlaySettingsWidget::setData(const ExportImageOverlaySettings& data)
{
    m_data = data;
    updateControls();
    emit dataChanged(m_data);
}

int ImageOverlaySettingsWidget::maximumWidth() const
{
    return ui->sizeSlider->maximum();
}

void ImageOverlaySettingsWidget::setMaximumWidth(int value)
{
    return ui->sizeSlider->setMaximum(value);
}

} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
