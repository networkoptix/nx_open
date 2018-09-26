#include "export_layout_settings_widget.h"
#include "ui_export_layout_settings_widget.h"

#include <nx/client/desktop/common/widgets/password_preview_button.h>

namespace nx {
namespace client {
namespace desktop {

ExportLayoutSettingsWidget::ExportLayoutSettingsWidget(QWidget* parent):
    base_type(parent),
    ui(new Ui::ExportLayoutSettingsWidget())
{
    ui->setupUi(this);

    connect(ui->readOnlyCheckBox, &QCheckBox::clicked,
        this, &ExportLayoutSettingsWidget::emitDataChanged);
}

ExportLayoutSettingsWidget::~ExportLayoutSettingsWidget()
{
}

void ExportLayoutSettingsWidget::emitDataChanged()
{
    Data data;
    data.readOnly = ui->readOnlyCheckBox->isChecked();
    emit dataChanged(data);
}

void ExportLayoutSettingsWidget::setData(const Data& data)
{
    ui->readOnlyCheckBox->setChecked(data.readOnly);
}

QLayout* ExportLayoutSettingsWidget::passwordPlaceholder()
{
    return ui->passwordWidget->layout();
}

} // namespace desktop
} // namespace client
} // namespace nx
