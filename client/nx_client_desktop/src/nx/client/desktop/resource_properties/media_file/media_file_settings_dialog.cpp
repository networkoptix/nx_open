#include "media_file_settings_dialog.h"
#include "ui_media_file_settings_dialog.h"

#include <QtCore/QScopedValueRollback>

#include <core/resource/media_resource.h>
#include <core/ptz/media_dewarping_params.h>

#include <ui/graphics/items/resource/resource_widget.h>
#include <ui/graphics/items/resource/media_resource_widget.h>

#include <nx/client/desktop/resource_properties/fisheye/fisheye_calibration_widget.h>

#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_item.h>

#include <utils/ffmpeg_image_provider.h>

namespace nx {
namespace client {
namespace desktop {

MediaFileSettingsDialog::MediaFileSettingsDialog(QWidget* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::MediaFileSettingsDialog),
    m_updating(false)
{
    ui->setupUi(this);
    connect(ui->fisheyeWidget, &FisheyeSettingsWidget::dataChanged,
        this, &MediaFileSettingsDialog::paramsChanged);
}

MediaFileSettingsDialog::~MediaFileSettingsDialog()
{
}

void MediaFileSettingsDialog::updateFromResource(const QnMediaResourcePtr& resource)
{
    if (m_resource == resource)
        return;

    QScopedValueRollback<bool> updateRollback(m_updating, true);

    m_resource = resource;
    auto resourcePtr = resource->toResourcePtr();

    if (resourcePtr->hasFlags(Qn::still_image))
        m_imageProvider.reset(new QnBasicImageProvider(QImage(resourcePtr->getUrl()), this));
    else
        m_imageProvider.reset(new QnFfmpegImageProvider(resourcePtr, this));

    m_imageProvider->loadAsync();

    ui->fisheyeWidget->updateFromParams(resource->getDewarpingParams(),
        m_imageProvider.data());
}

void MediaFileSettingsDialog::submitToResource(const QnMediaResourcePtr& resource)
{
    QnMediaDewarpingParams params = resource->getDewarpingParams();
    ui->fisheyeWidget->submitToParams(params);
    resource->setDewarpingParams(params);
}

void MediaFileSettingsDialog::paramsChanged()
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

} // namespace desktop
} // namespace client
} // namespace nx
