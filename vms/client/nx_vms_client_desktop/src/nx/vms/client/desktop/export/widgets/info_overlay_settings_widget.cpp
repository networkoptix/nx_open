// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "info_overlay_settings_widget.h"
#include "ui_info_overlay_settings_widget.h"

#include <limits>

#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <ui/workaround/widgets_signals_workaround.h>

namespace nx::vms::client::desktop {

namespace {

static constexpr int kMaximumFontSize = 400;

static const QColor klight16Color = "#698796";
static const nx::vms::client::core::SvgIconColorer::IconSubstitutions kIconSubstitutions = {
    {QIcon::Normal, {{klight16Color, "light16"}}},
    {QIcon::Active, {{klight16Color, "light17"}}},
    {QIcon::Selected, {{klight16Color, "light15"}}},
};

} // namespace

InfoOverlaySettingsWidget::InfoOverlaySettingsWidget(QWidget* parent):
    base_type(parent),
    ui(new Ui::InfoOverlaySettingsWidget())
{
    ui->setupUi(this);

    ui->fontSizeSlider->setProperty(style::Properties::kSliderFeatures,
        static_cast<int>(style::SliderFeature::FillingUp));

    ui->fontSizeSlider->setMaximum(std::numeric_limits<int>::max());

    // Synchronize slider with spin box.
    connect(ui->fontSizeSlider, &QSlider::rangeChanged, ui->fontSizeSpinBox, &QSpinBox::setRange);
    connect(ui->fontSizeSlider, &QSlider::valueChanged, ui->fontSizeSpinBox, &QSpinBox::setValue);
    connect(ui->fontSizeSpinBox, QnSpinboxIntValueChanged, ui->fontSizeSlider, &QSlider::setValue);

    ui->fontSizeSlider->setRange(ExportTextOverlayPersistentSettings::minimumFontSize(),
        kMaximumFontSize);
    updateControls();

    connect(ui->fontSizeSpinBox, QnSpinboxIntValueChanged,
        [this](int value)
        {
            m_data.fontSize = value;
            emit dataChanged(m_data);
        });

    connect(ui->fontSizeSlider, &QSlider::valueChanged,
        [this](int value)
        {
            m_data.fontSize = value;
            emit dataChanged(m_data);
        });

    connect(ui->exportCameraNameCheckBox, &QCheckBox::stateChanged,
        [this](int state)
        {
            m_data.exportCameraName = state;
            emit dataChanged(m_data);
        });

    connect(ui->exportDateCheckBox, &QCheckBox::stateChanged,
        [this](int state)
        {
            m_data.exportDate = state;
            emit dataChanged(m_data);
        });

    ui->deleteButton->setIcon(qnSkin->icon("text_buttons/delete_20.svg", kIconSubstitutions));

    connect(ui->deleteButton, &QPushButton::clicked,
        this, &InfoOverlaySettingsWidget::deleteClicked);
}

InfoOverlaySettingsWidget::~InfoOverlaySettingsWidget()
{
}

void InfoOverlaySettingsWidget::updateControls()
{
    ui->exportCameraNameCheckBox->setChecked(m_data.exportCameraName);
    ui->exportDateCheckBox->setChecked(m_data.exportDate);
    ui->fontSizeSpinBox->setValue(m_data.fontSize);
}

const ExportInfoOverlayPersistentSettings& InfoOverlaySettingsWidget::data() const
{
    return m_data;
}

void InfoOverlaySettingsWidget::setData(const ExportInfoOverlayPersistentSettings& data)
{
    m_data = data;
    updateControls();
    emit dataChanged(m_data);
}

int InfoOverlaySettingsWidget::maxOverlayWidth() const
{
    return m_data.maxOverlayWidth;
}

void InfoOverlaySettingsWidget::setMaxOverlayWidth(int value)
{
    m_data.maxOverlayWidth = value;
}

int InfoOverlaySettingsWidget::maxOverlayHeight() const
{
    return m_data.maxOverlayHeight;
}

void InfoOverlaySettingsWidget::setMaxOverlayHeight(int value)
{
    m_data.maxOverlayHeight = value;
}

} // namespace nx::vms::client::desktop
