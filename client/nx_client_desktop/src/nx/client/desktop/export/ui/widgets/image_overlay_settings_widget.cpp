#include "image_overlay_settings_widget.h"
#include "ui_image_overlay_settings_widget.h"

#include <ui/common/aligner.h>

namespace nx {
namespace client {
namespace desktop {
namespace ui {

ImageOverlaySettingsWidget::ImageOverlaySettingsWidget(QWidget* parent):
    base_type(parent),
    ui(new Ui::ImageOverlaySettingsWidget())
{
    ui->setupUi(this);

    auto aligner = new QnAligner(this);
    aligner->addWidgets({ ui->sizeLabel, ui->opacityLabel });
}

} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
