#include "timestamp_overlay_settings_widget.h"
#include "ui_timestamp_overlay_settings_widget.h"

#include <ui/common/aligner.h>
#include <ui/workaround/widgets_signals_workaround.h>

namespace nx {
namespace client {
namespace desktop {
namespace ui {

namespace {

static constexpr int kMinimumFontSize = 8;
static constexpr int kMaximumFontSize = 48;

} // namespace

TimestampOverlaySettingsWidget::TimestampOverlaySettingsWidget(QWidget* parent):
    base_type(parent),
    ui(new Ui::TimestampOverlaySettingsWidget())
{
    ui->setupUi(this);

    auto aligner = new QnAligner(this);
    aligner->addWidget(ui->fontSizeLabel);

    ui->formatComboBox->addItem(tr("Long"), Qt::DefaultLocaleLongDate);
    ui->formatComboBox->addItem(tr("Short"), Qt::DefaultLocaleShortDate);
    ui->formatComboBox->addItem(lit("ISO 8601"), Qt::ISODate);
    ui->formatComboBox->addItem(lit("RFC 2822"), Qt::RFC2822Date);

    ui->fontSizeSpinBox->setRange(kMinimumFontSize, kMaximumFontSize);
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
            m_data.format = static_cast<Qt::DateFormat>(ui->formatComboBox->itemData(index).toInt());
            emit dataChanged(m_data);
        });
}

TimestampOverlaySettingsWidget::~TimestampOverlaySettingsWidget()
{
}

void TimestampOverlaySettingsWidget::updateControls()
{
    ui->fontSizeSpinBox->setValue(m_data.fontSize);
    ui->formatComboBox->setCurrentIndex(ui->formatComboBox->findData(m_data.format));
}

const ExportTimestampOverlaySettings& TimestampOverlaySettingsWidget::data() const
{
    return m_data;
}

void TimestampOverlaySettingsWidget::setData(const ExportTimestampOverlaySettings& data)
{
    m_data = data;
    updateControls();
    emit dataChanged(m_data);
}

} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
