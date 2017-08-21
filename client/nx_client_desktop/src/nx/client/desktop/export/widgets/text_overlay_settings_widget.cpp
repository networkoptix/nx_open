#include "text_overlay_settings_widget.h"
#include "ui_text_overlay_settings_widget.h"

#include <ui/common/aligner.h>

namespace nx {
namespace client {
namespace desktop {
namespace ui {

TextOverlaySettingsWidget::TextOverlaySettingsWidget(QWidget* parent):
    base_type(parent),
    ui(new Ui::TextOverlaySettingsWidget())
{
    ui->setupUi(this);

    auto aligner = new QnAligner(this);
    aligner->addWidgets({ui->widthLabel, ui->fontSizeLabel});

    connect(ui->plainTextEdit, &QPlainTextEdit::textChanged,
        [this]() { emit dataChanged(); });

    connect(ui->fontSizeSlider, &QAbstractSlider::valueChanged,
        this, &TextOverlaySettingsWidget::dataChanged);

    connect(ui->widthSlider, &QAbstractSlider::valueChanged,
        this, &TextOverlaySettingsWidget::dataChanged);
}

QString TextOverlaySettingsWidget::text() const
{
    return ui->plainTextEdit->toPlainText();
}

void TextOverlaySettingsWidget::setText(const QString& value)
{
    ui->plainTextEdit->setPlainText(value);
}

} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
