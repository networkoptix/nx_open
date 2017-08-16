#include "timestamp_overlay_settings_widget.h"
#include "ui_timestamp_overlay_settings_widget.h"

namespace nx {
namespace client {
namespace desktop {
namespace ui {

TimestampOverlaySettingsWidget::TimestampOverlaySettingsWidget(QWidget* parent):
    base_type(parent),
    ui(new Ui::TimestampOverlaySettingsWidget())
{
    ui->setupUi(this);
}

} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
