#include "picture_settings_dialog.h"
#include "ui_picture_settings_dialog.h"

#include <core/resource/media_resource.h>
#include <core/ptz/media_dewarping_params.h>

#include <ui/graphics/items/resource/resource_widget.h>
#include <ui/graphics/items/resource/media_resource_widget.h>

#include <ui/widgets/fisheye/fisheye_calibration_widget.h>

#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_display.h>

#include <utils/image_provider.h>

QnPictureSettingsDialog::QnPictureSettingsDialog(QWidget *parent) :
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::QnPictureSettingsDialog)
{
    ui->setupUi(this);

    connect(ui->fisheyeCheckBox, &QCheckBox::toggled, this, &QnPictureSettingsDialog::at_fisheyeCheckbox_toggled);
}

QnPictureSettingsDialog::~QnPictureSettingsDialog()
{}

void QnPictureSettingsDialog::updateFromResource(const QnMediaResourcePtr &resource) {
    QImage image(resource->toResource()->getUrl());
    ui->fisheyeCheckBox->setEnabled(!image.isNull());

    ui->imageLabel->setPixmap(QPixmap::fromImage(image).scaled(ui->imageLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));

    QnMediaDewarpingParams params = resource->getDewarpingParams();
    ui->fisheyeCheckBox->setChecked(params.enabled);
    ui->fisheyeWidget->updateFromParams(params, new QnBasicImageProvider(image, this));
}

void QnPictureSettingsDialog::submitToResource(const QnMediaResourcePtr &resource) {
    if (!ui->fisheyeCheckBox->isEnabled())
        return;

    QnMediaDewarpingParams params = resource->getDewarpingParams();
    ui->fisheyeWidget->submitToParams(params);
    params.enabled = ui->fisheyeCheckBox->isChecked();
    resource->setDewarpingParams(params);
}

void QnPictureSettingsDialog::at_fisheyeCheckbox_toggled(bool checked) {
    ui->stackedWidget->setCurrentWidget(checked ? ui->fisheyePage : ui->imagePage);
}

void QnPictureSettingsDialog::paramsChanged() {
    QnResourceWidget* centralWidget = display()->widget(Qn::CentralRole);
    if (QnMediaResourceWidget* mediaWidget = dynamic_cast<QnMediaResourceWidget*>(centralWidget)) {
        QnMediaDewarpingParams dewarpingParams = mediaWidget->dewarpingParams();
        ui->fisheyeWidget->submitToParams(dewarpingParams);
        dewarpingParams.enabled = ui->fisheyeCheckBox->isChecked();
        mediaWidget->setDewarpingParams(dewarpingParams);
    }

}
