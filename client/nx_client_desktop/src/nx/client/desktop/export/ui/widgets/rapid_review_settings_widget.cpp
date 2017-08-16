#include "rapid_review_settings_widget.h"
#include "ui_rapid_review_settings_widget.h"

namespace nx {
namespace client {
namespace desktop {
namespace ui {

RapidReviewSettingsWidget::RapidReviewSettingsWidget(QWidget* parent):
    base_type(parent),
    ui(new Ui::RapidReviewSettingsWidget())
{
    ui->setupUi(this);
}

} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
