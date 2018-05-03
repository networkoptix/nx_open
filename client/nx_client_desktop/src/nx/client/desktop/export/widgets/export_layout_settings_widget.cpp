#include "export_layout_settings_widget.h"
#include "ui_export_layout_settings_widget.h"

namespace nx {
namespace client {
namespace desktop {

ExportLayoutSettingsWidget::ExportLayoutSettingsWidget(QWidget* parent):
    base_type(parent),
    ui(new Ui::ExportLayoutSettingsWidget())
{
    ui->setupUi(this);
    connect(ui->readOnlyCheckBox, &QCheckBox::toggled,
        this, &ExportLayoutSettingsWidget::dataChanged);
}

ExportLayoutSettingsWidget::~ExportLayoutSettingsWidget()
{
}

bool ExportLayoutSettingsWidget::isLayoutReadOnly() const
{
    return ui->readOnlyCheckBox->isChecked();
}

void ExportLayoutSettingsWidget::setLayoutReadOnly(bool value)
{
    ui->readOnlyCheckBox->setChecked(value);
}

} // namespace desktop
} // namespace client
} // namespace nx
