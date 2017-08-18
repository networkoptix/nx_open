#include "export_media_settings_widget.h"
#include "ui_export_media_settings_widget.h"

namespace nx {
namespace client {
namespace desktop {
namespace ui {

ExportMediaSettingsWidget::ExportMediaSettingsWidget(QWidget* parent):
    base_type(parent),
    ui(new Ui::ExportMediaSettingsWidget())
{
    ui->setupUi(this);
}

} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
