#include "export_media_settings_widget.h"
#include "ui_export_media_settings_widget.h"

#include <QtWidgets/QStyle>

#include <nx/client/desktop/common/widgets/password_preview_button.h>

namespace nx {
namespace client {
namespace desktop {

ExportMediaSettingsWidget::ExportMediaSettingsWidget(QWidget* parent):
    base_type(parent),
    ui(new Ui::ExportMediaSettingsWidget())
{
    ui->setupUi(this);

    connect(ui->filtersCheckBox, &QCheckBox::clicked,
        this, &ExportMediaSettingsWidget::emitDataChanged);
}

ExportMediaSettingsWidget::~ExportMediaSettingsWidget()
{
}

void ExportMediaSettingsWidget::emitDataChanged()
{
    Data data;
    data.applyFilters = ui->filtersCheckBox->isChecked();
    emit dataChanged(data);
}

void ExportMediaSettingsWidget::setData(const Data& data)
{
    ui->filtersCheckBox->setEnabled(data.transcodingAllowed);
    ui->filtersCheckBox->setChecked(data.applyFilters);
}

QLayout* ExportMediaSettingsWidget::passwordPlaceholder()
{
    return ui->passwordWidget->layout();
}

} // namespace desktop
} // namespace client
} // namespace nx
