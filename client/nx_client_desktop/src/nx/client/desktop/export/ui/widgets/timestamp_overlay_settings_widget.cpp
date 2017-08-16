#include "timestamp_overlay_settings_widget.h"
#include "ui_timestamp_overlay_settings_widget.h"

#include <ui/common/aligner.h>

namespace nx {
namespace client {
namespace desktop {
namespace ui {

TimestampOverlaySettingsWidget::TimestampOverlaySettingsWidget(QWidget* parent):
    base_type(parent),
    ui(new Ui::TimestampOverlaySettingsWidget())
{
    ui->setupUi(this);

    auto aligner = new QnAligner(this);
    aligner->addWidget(ui->sizeLabel);
}

} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
