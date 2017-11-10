#include "camera_info_widget.h"
#include "ui_camera_info_widget.h"

namespace nx {
namespace client {
namespace desktop {

CameraInfoWidget::CameraInfoWidget(QWidget* parent):
    base_type(parent),
    ui(new Ui::CameraInfoWidget())
{
    ui->setupUi(this);
}

CameraInfoWidget::~CameraInfoWidget()
{
}

} // namespace desktop
} // namespace client
} // namespace nx
