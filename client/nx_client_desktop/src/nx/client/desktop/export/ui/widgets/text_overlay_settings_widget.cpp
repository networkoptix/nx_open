#include "text_overlay_settings_widget.h"
#include "ui_text_overlay_settings_widget.h"

#include <ui/common/aligner.h>

namespace nx {
namespace client {
namespace desktop {
namespace ui {

TextOverlaySettingsWidget::TextOverlaySettingsWidget(QWidget* parent):
    base_type(parent),
    ui(new Ui::TextOverlaySettingsWidget())
{
    ui->setupUi(this);

    auto aligner = new QnAligner(this);
    aligner->addWidgets({ui->widthLabel, ui->fontSizeLabel});
}

} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
