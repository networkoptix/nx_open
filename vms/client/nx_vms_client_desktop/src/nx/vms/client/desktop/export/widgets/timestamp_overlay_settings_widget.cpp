// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "timestamp_overlay_settings_widget.h"
#include "ui_timestamp_overlay_settings_widget.h"

#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/common/utils/aligner.h>
#include <ui/workaround/widgets_signals_workaround.h>

using nx::core::transcoding::TimestampFormat;

namespace nx::vms::client::desktop {

namespace {

static constexpr int kMaximumFontSize = 400;

const nx::vms::client::core::SvgIconColorer::ThemeSubstitutions kThemeSubstitutions = {
    {QIcon::Normal, {.primary = "light16"}},
    {QIcon::Active, {.primary = "light17"}},
    {QIcon::Selected, {.primary = "light15"}},
};

NX_DECLARE_COLORIZED_ICON(kDeleteIcon, "20x20/Outline/delete.svg", kThemeSubstitutions)

} // namespace

TimestampOverlaySettingsWidget::TimestampOverlaySettingsWidget(QWidget* parent):
    base_type(parent),
    ui(new Ui::TimestampOverlaySettingsWidget())
{
    ui->setupUi(this);

    auto aligner = new Aligner(this);
    aligner->addWidgets({ui->fontSizeLabel, ui->formatLabel});

    ui->formatComboBox->addItem(tr("Long"),
        QVariant::fromValue(TimestampFormat::longDate));
    ui->formatComboBox->addItem(tr("Short"),
        QVariant::fromValue(TimestampFormat::shortDate));
    ui->formatComboBox->addItem("ISO 8601",
        QVariant::fromValue(TimestampFormat::ISODate));
    ui->formatComboBox->addItem("RFC 2822",
        QVariant::fromValue(TimestampFormat::RFC2822Date));

    ui->fontSizeSpinBox->setRange(ExportTimestampOverlayPersistentSettings::minimumFontSize(),
        kMaximumFontSize);
    updateControls();

    connect(ui->fontSizeSpinBox, QnSpinboxIntValueChanged,
        [this](int value)
        {
            m_data.fontSize = value;
            emit dataChanged(m_data);
        });

    connect(ui->formatComboBox, QnComboboxCurrentIndexChanged,
        [this](int index)
        {
            auto format = static_cast<TimestampFormat>(
                ui->formatComboBox->itemData(index).toInt());
            emit formatChanged(format);
        });

    ui->deleteButton->setIcon(qnSkin->icon(kDeleteIcon));

    connect(ui->deleteButton, &QPushButton::clicked,
        this, &TimestampOverlaySettingsWidget::deleteClicked);
}

TimestampOverlaySettingsWidget::~TimestampOverlaySettingsWidget()
{
}

void TimestampOverlaySettingsWidget::updateControls()
{
    ui->fontSizeSpinBox->setValue(m_data.fontSize);
    ui->fontSizeSpinBox->setMaximum(m_data.maximumFontSize);
    ui->formatComboBox->setCurrentIndex(ui->formatComboBox->findData(
        QVariant::fromValue(m_data.format)));
}

const ExportTimestampOverlayPersistentSettings& TimestampOverlaySettingsWidget::data() const
{
    return m_data;
}

void TimestampOverlaySettingsWidget::setData(const ExportTimestampOverlayPersistentSettings& data)
{
    m_data = data;
    updateControls();
    emit dataChanged(m_data);
}

bool TimestampOverlaySettingsWidget::formatEnabled() const
{
    return ui->formatComboBox->isEnabled();
}

void TimestampOverlaySettingsWidget::setFormatEnabled(bool value)
{
    ui->formatLabel->setEnabled(value);
    ui->formatComboBox->setEnabled(value);
}

} // namespace nx::vms::client::desktop
