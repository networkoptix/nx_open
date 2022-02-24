// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "bookmark_overlay_settings_widget.h"
#include "ui_bookmark_overlay_settings_widget.h"

#include <limits>
#include <nx/vms/client/desktop/common/utils/aligner.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/style/skin.h>
#include <ui/workaround/widgets_signals_workaround.h>

namespace nx::vms::client::desktop {

namespace {

static constexpr int kMinimumWidth = 60;

static constexpr int kMaximumFontSize = 400;

} // namespace

BookmarkOverlaySettingsWidget::BookmarkOverlaySettingsWidget(QWidget* parent):
    base_type(parent),
    ui(new Ui::BookmarkOverlaySettingsWidget())
{
    ui->setupUi(this);

    ui->widthSlider->setProperty(style::Properties::kSliderFeatures,
        static_cast<int>(style::SliderFeature::FillingUp));

    ui->widthSlider->setMaximum(std::numeric_limits<int>::max());

    auto aligner = new Aligner(this);
    aligner->addWidgets({ui->widthLabel, ui->fontSizeLabel});

    // Synchronize slider with spin box.
    connect(ui->widthSlider, &QSlider::rangeChanged, ui->widthSpinBox, &QSpinBox::setRange);
    connect(ui->widthSlider, &QSlider::valueChanged, ui->widthSpinBox, &QSpinBox::setValue);
    connect(ui->widthSpinBox, QnSpinboxIntValueChanged, ui->widthSlider, &QSlider::setValue);

    ui->widthSlider->setRange(kMinimumWidth, m_data.overlayWidth * 2);
    ui->fontSizeSpinBox->setRange(ExportBookmarkOverlayPersistentSettings::minimumFontSize(),
        kMaximumFontSize);
    updateControls();

    connect(ui->descriptionCheckBox, &QCheckBox::stateChanged,
        [this]()
        {
            m_data.includeDescription = ui->descriptionCheckBox->isChecked();
            emit dataChanged(m_data);
        });

    connect(ui->fontSizeSpinBox, QnSpinboxIntValueChanged,
        [this](int value)
        {
            m_data.fontSize = value;
            emit dataChanged(m_data);
        });

    connect(ui->widthSlider, &QSlider::valueChanged,
        [this](int value)
        {
            m_data.overlayWidth = value;
            emit dataChanged(m_data);
        });

    ui->deleteButton->setIcon(qnSkin->icon(lit("text_buttons/trash.png")));

    connect(ui->deleteButton, &QPushButton::clicked,
        this, &BookmarkOverlaySettingsWidget::deleteClicked);
}

BookmarkOverlaySettingsWidget::~BookmarkOverlaySettingsWidget()
{
}

void BookmarkOverlaySettingsWidget::updateControls()
{
    ui->fontSizeSpinBox->setValue(m_data.fontSize);
    ui->widthSlider->setValue(m_data.overlayWidth);
    ui->descriptionCheckBox->setChecked(m_data.includeDescription);
}

const ExportBookmarkOverlayPersistentSettings& BookmarkOverlaySettingsWidget::data() const
{
    return m_data;
}

void BookmarkOverlaySettingsWidget::setData(const ExportBookmarkOverlayPersistentSettings& data)
{
    m_data = data;
    updateControls();
    emit dataChanged(m_data);
}

int BookmarkOverlaySettingsWidget::maxOverlayWidth() const
{
    return ui->widthSlider->maximum();
}

void BookmarkOverlaySettingsWidget::setMaxOverlayWidth(int value)
{
    ui->widthSlider->setMaximum(value);
}

} // namespace nx::vms::client::desktop
