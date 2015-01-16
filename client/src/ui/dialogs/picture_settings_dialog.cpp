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
#include <ui/workbench/workbench_item.h>

#include <utils/image_provider.h>
#include <utils/common/scoped_value_rollback.h>

QnPictureSettingsDialog::QnPictureSettingsDialog(QWidget *parent) :
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::QnPictureSettingsDialog),
    m_updating(false)
{
    ui->setupUi(this);

    connect(ui->fisheyeCheckBox,    &QCheckBox::toggled,                    this,   &QnPictureSettingsDialog::at_fisheyeCheckbox_toggled);

    connect(ui->fisheyeCheckBox,    &QCheckBox::toggled,                    this,   &QnPictureSettingsDialog::paramsChanged);
    connect(ui->fisheyeWidget,      &QnFisheyeSettingsWidget::dataChanged,  this,   &QnPictureSettingsDialog::paramsChanged);
}

QnPictureSettingsDialog::~QnPictureSettingsDialog()
{}

void QnPictureSettingsDialog::updateFromResource(const QnMediaResourcePtr &resource) {
    QN_SCOPED_VALUE_ROLLBACK(&m_updating, true);

    m_resource = resource;

    QImage image(resource->toResource()->getUrl());
    ui->fisheyeCheckBox->setEnabled(!image.isNull());

    ui->imageLabel->setPixmap(QPixmap::fromImage(image).scaled(ui->imageLabel->contentSize(), Qt::KeepAspectRatio, Qt::SmoothTransformation));

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
    if (m_updating)
        return;

    /* Dialog is modal, so changes rollback is not required. */
    QnResourceWidget* centralWidget = display()->widget(Qn::CentralRole);
    QnMediaResourceWidget* mediaWidget = dynamic_cast<QnMediaResourceWidget*>(centralWidget);
    if (!m_resource || !mediaWidget || mediaWidget->resource() != m_resource)
        return;

    QnMediaDewarpingParams dewarpingParams = mediaWidget->dewarpingParams();
    ui->fisheyeWidget->submitToParams(dewarpingParams);
    dewarpingParams.enabled = ui->fisheyeCheckBox->isChecked();
    mediaWidget->setDewarpingParams(dewarpingParams);

    QnWorkbenchItem *item = mediaWidget->item();
    QnItemDewarpingParams itemParams = item->dewarpingParams();
    itemParams.enabled = dewarpingParams.enabled;
    item->setDewarpingParams(itemParams);
}
