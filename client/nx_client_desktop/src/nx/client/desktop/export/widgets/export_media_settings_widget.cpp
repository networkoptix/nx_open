#include "export_media_settings_widget.h"
#include "ui_export_media_settings_widget.h"

namespace nx {
namespace client {
namespace desktop {

ExportMediaSettingsWidget::ExportMediaSettingsWidget(QWidget* parent):
    base_type(parent),
    ui(new Ui::ExportMediaSettingsWidget())
{
    ui->setupUi(this);
    connect(ui->filtersCheckBox, &QCheckBox::toggled,
        this, &ExportMediaSettingsWidget::dataChanged);
}

ExportMediaSettingsWidget::~ExportMediaSettingsWidget()
{
}

bool ExportMediaSettingsWidget::applyFilters() const
{
    return ui->filtersCheckBox->isChecked();
}

void ExportMediaSettingsWidget::setApplyFilters(bool value)
{
    ui->filtersCheckBox->setChecked(value);
}

} // namespace desktop
} // namespace client
} // namespace nx
