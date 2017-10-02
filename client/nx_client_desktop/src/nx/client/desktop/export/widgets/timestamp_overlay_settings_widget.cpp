#include "timestamp_overlay_settings_widget.h"
#include "ui_timestamp_overlay_settings_widget.h"

#include <ui/common/aligner.h>
#include <ui/style/skin.h>
#include <ui/workaround/widgets_signals_workaround.h>

namespace nx {
namespace client {
namespace desktop {

namespace {

static constexpr int kMinimumFontSize = 10;
static constexpr int kMaximumFontSize = 400;

} // namespace

TimestampOverlaySettingsWidget::TimestampOverlaySettingsWidget(QWidget* parent):
    base_type(parent),
    ui(new Ui::TimestampOverlaySettingsWidget())
{
    ui->setupUi(this);

    auto aligner = new QnAligner(this);
    aligner->addWidgets({ui->fontSizeLabel, ui->formatLabel});

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

    ui->deleteButton->setIcon(qnSkin->icon(lit("buttons/trash.png")));

    connect(ui->deleteButton, &QPushButton::clicked,
        this, &TimestampOverlaySettingsWidget::deleteClicked);
}

TimestampOverlaySettingsWidget::~TimestampOverlaySettingsWidget()
{
}

void TimestampOverlaySettingsWidget::updateControls()
{
    ui->fontSizeSpinBox->setValue(m_data.fontSize);
    ui->formatComboBox->setCurrentIndex(ui->formatComboBox->findData(m_data.format));
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

} // namespace desktop
} // namespace client
} // namespace nx
