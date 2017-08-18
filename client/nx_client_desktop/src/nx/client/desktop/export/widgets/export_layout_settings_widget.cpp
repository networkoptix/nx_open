#include "export_layout_settings_widget.h"
#include "ui_export_layout_settings_widget.h"

namespace nx {
namespace client {
namespace desktop {
namespace ui {

ExportLayoutSettingsWidget::ExportLayoutSettingsWidget(QWidget* parent):
    base_type(parent),
    ui(new Ui::ExportLayoutSettingsWidget())
{
    ui->setupUi(this);
}

} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
