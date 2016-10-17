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

QnMediaFileSettingsDialog::QnMediaFileSettingsDialog(QWidget* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::QnMediaFileSettingsDialog),
    m_updating(false)
{
    ui->setupUi(this);
    connect(ui->fisheyeWidget, &QnFisheyeSettingsWidget::dataChanged,
        this, &QnMediaFileSettingsDialog::paramsChanged);
}

QnMediaFileSettingsDialog::~QnMediaFileSettingsDialog()
{
}

void QnMediaFileSettingsDialog::updateFromResource(const QnMediaResourcePtr& resource)
{
    if (m_resource == resource)
        return;

    QN_SCOPED_VALUE_ROLLBACK(&m_updating, true);

    m_resource = resource;

    if (m_resource->toResourcePtr()->hasFlags(Qn::still_image))
    {
        m_imageProvider.reset(new QnBasicImageProvider(
            QImage(m_resource->toResourcePtr()->getUrl()), this));
    }
    else
    {
        m_imageProvider.reset(new QnFfmpegImageProvider(resource->toResourcePtr(), this));
    }

    m_imageProvider->loadAsync();

    ui->fisheyeWidget->updateFromParams(resource->getDewarpingParams(),
        m_imageProvider.data());
}

void QnMediaFileSettingsDialog::submitToResource(const QnMediaResourcePtr& resource)
{
    QnMediaDewarpingParams params = resource->getDewarpingParams();
    ui->fisheyeWidget->submitToParams(params);
    resource->setDewarpingParams(params);
}

void QnMediaFileSettingsDialog::paramsChanged()
{
    if (m_updating)
        return;

    /* Dialog is modal, so changes rollback is not required. */
    QnResourceWidget* centralWidget = display()->widget(Qn::CentralRole);
    QnMediaResourceWidget* mediaWidget = dynamic_cast<QnMediaResourceWidget*>(centralWidget);
    if (!m_resource || !mediaWidget || mediaWidget->resource() != m_resource)
        return;

    QnMediaDewarpingParams dewarpingParams = mediaWidget->dewarpingParams();
    ui->fisheyeWidget->submitToParams(dewarpingParams);
    mediaWidget->setDewarpingParams(dewarpingParams);

    QnWorkbenchItem* item = mediaWidget->item();
    QnItemDewarpingParams itemParams = item->dewarpingParams();
    itemParams.enabled = dewarpingParams.enabled;
    item->setDewarpingParams(itemParams);
}
