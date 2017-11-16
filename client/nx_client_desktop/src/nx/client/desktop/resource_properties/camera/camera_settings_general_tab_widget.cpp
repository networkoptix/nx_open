#include "camera_settings_general_tab_widget.h"
#include "ui_camera_settings_general_tab_widget.h"

namespace nx {
namespace client {
namespace desktop {

CameraSettingsGeneralTabWidget::CameraSettingsGeneralTabWidget(QWidget* parent):
    base_type(parent),
    ui(new Ui::CameraSettingsGeneralTabWidget())
{
    ui->setupUi(this);
}

CameraSettingsGeneralTabWidget::~CameraSettingsGeneralTabWidget()
{
}

} // namespace desktop
} // namespace client
} // namespace nx
