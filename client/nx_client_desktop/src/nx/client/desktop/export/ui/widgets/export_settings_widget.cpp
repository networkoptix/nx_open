#include "export_settings_widget.h"
#include "ui_export_settings_widget.h"

namespace nx {
namespace client {
namespace desktop {
namespace ui {

ExportSettingsWidget::ExportSettingsWidget(QWidget* parent):
    base_type(parent),
    ui(new Ui::ExportSettingsWidget())
{
    ui->setupUi(this);
}

} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
