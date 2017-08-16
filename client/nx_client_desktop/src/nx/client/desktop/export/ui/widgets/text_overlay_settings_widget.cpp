#include "text_overlay_settings_widget.h"
#include "ui_text_overlay_settings_widget.h"

namespace nx {
namespace client {
namespace desktop {
namespace ui {

TextOverlaySettingsWidget::TextOverlaySettingsWidget(QWidget* parent):
    base_type(parent),
    ui(new Ui::TextOverlaySettingsWidget())
{
    ui->setupUi(this);
}

} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
