#include "fisheye_settings_widget.h"
#include "ui_fisheye_settings_widget.h"

#include <ui/style/skin.h>
#include "core/resource/resource_type.h"

QnFisheyeSettingsWidget::QnFisheyeSettingsWidget(QWidget* parent):
    QWidget(parent),
    ui(new Ui::FisheyeSettingsWidget)
{
    ui->setupUi(this);
}

QnFisheyeSettingsWidget::~QnFisheyeSettingsWidget()
{

}

void QnFisheyeSettingsWidget::updateFromResource(QnSecurityCamResourcePtr camera)
{
    if (!camera)
        return;
}
