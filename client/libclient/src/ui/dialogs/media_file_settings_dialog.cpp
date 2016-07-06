#include "media_file_settings_dialog.h"
#include "ui_media_file_settings_dialog.h"

#include <core/resource/media_resource.h>
#include <core/ptz/media_dewarping_params.h>

#include <ui/graphics/items/resource/resource_widget.h>
#include <ui/graphics/items/resource/media_resource_widget.h>

#include <ui/widgets/fisheye/fisheye_calibration_widget.h>

#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_item.h>

#include <utils/ffmpeg_image_provider.h>
#include <utils/common/scoped_value_rollback.h>

QnMediaFileSettingsDialog::QnMediaFileSettingsDialog(QWidget *parent) :
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::QnMediaFileSettingsDialog),
    m_updating(false),
    m_imageProvider(NULL)
{
    ui->setupUi(this);

    connect(ui->fisheyeCheckBox,    &QCheckBox::toggled,                    this,   &QnMediaFileSettingsDialog::at_fisheyeCheckbox_toggled);

    connect(ui->fisheyeCheckBox,    &QCheckBox::toggled,                    this,   &QnMediaFileSettingsDialog::paramsChanged);
    connect(ui->fisheyeWidget,      &QnFisheyeSettingsWidget::dataChanged,  this,   &QnMediaFileSettingsDialog::paramsChanged);

    //File Settings....
}

QnMediaFileSettingsDialog::~QnMediaFileSettingsDialog()
{}

void QnMediaFileSettingsDialog::updateFromResource(const QnMediaResourcePtr &resource) {
    QN_SCOPED_VALUE_ROLLBACK(&m_updating, true);

    if (m_imageProvider) {
        delete m_imageProvider;
        m_imageProvider = NULL;
    }

    m_resource = resource;

    if (m_resource->toResourcePtr()->hasFlags(Qn::still_image)) {
        QImage image(m_resource->toResourcePtr()->getUrl());
        m_imageProvider = new QnBasicImageProvider(image, this);
    } else {
        m_imageProvider = new QnFfmpegImageProvider(resource->toResourcePtr(), this);
    }

    connect(m_imageProvider, &QnImageProvider::imageChanged, this, [this](const QImage &image) {
        if (image.isNull())
            return;
        ui->imageLabel->setPixmap(QPixmap::fromImage(image).scaled(ui->imageLabel->contentSize(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    });
    m_imageProvider->loadAsync();

    QnMediaDewarpingParams params = resource->getDewarpingParams();
    ui->fisheyeCheckBox->setChecked(params.enabled);
    ui->fisheyeWidget->updateFromParams(params, m_imageProvider);
}

void QnMediaFileSettingsDialog::submitToResource(const QnMediaResourcePtr &resource) {
    if (!ui->fisheyeCheckBox->isEnabled())
        return;

    QnMediaDewarpingParams params = resource->getDewarpingParams();
    ui->fisheyeWidget->submitToParams(params);
    params.enabled = ui->fisheyeCheckBox->isChecked();
    resource->setDewarpingParams(params);
}

void QnMediaFileSettingsDialog::at_fisheyeCheckbox_toggled(bool checked) {
    ui->stackedWidget->setCurrentWidget(checked ? ui->fisheyePage : ui->imagePage);
}

void QnMediaFileSettingsDialog::paramsChanged() {
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
