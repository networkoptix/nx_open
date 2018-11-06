#include "export_layout_settings_widget.h"
#include "ui_export_layout_settings_widget.h"

#include <nx/vms/client/desktop/common/widgets/password_preview_button.h>

namespace nx::vms::client::desktop {

ExportLayoutSettingsWidget::ExportLayoutSettingsWidget(QWidget* parent):
    base_type(parent),
    ui(new Ui::ExportLayoutSettingsWidget())
{
    ui->setupUi(this);

    connect(ui->readOnlyCheckBox, &QCheckBox::clicked,
        this, &ExportLayoutSettingsWidget::emitDataEdited);
}

ExportLayoutSettingsWidget::~ExportLayoutSettingsWidget()
{
}

void ExportLayoutSettingsWidget::emitDataEdited()
{
    Data data;
    data.readOnly = ui->readOnlyCheckBox->isChecked();
    emit dataEdited(data);
}

void ExportLayoutSettingsWidget::setData(const Data& data)
{
    ui->readOnlyCheckBox->setChecked(data.readOnly);
}

QLayout* ExportLayoutSettingsWidget::passwordPlaceholder()
{
    return ui->passwordWidget->layout();
}

} // namespace nx::vms::client::desktop
