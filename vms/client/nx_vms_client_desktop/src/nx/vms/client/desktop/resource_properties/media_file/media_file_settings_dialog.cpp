#include "media_file_settings_dialog.h"
#include "ui_media_file_settings_dialog.h"

#include <QtCore/QScopedValueRollback>

#include <core/resource/media_resource.h>

#include <nx/vms/client/desktop/resource_properties/fisheye/fisheye_calibration_widget.h>

#include <nx/vms/client/desktop/image_providers/ffmpeg_image_provider.h>
#include "../fisheye/fisheye_preview_controller.h"

namespace nx::vms::client::desktop {

MediaFileSettingsDialog::MediaFileSettingsDialog(QWidget* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::MediaFileSettingsDialog)
{
    ui->setupUi(this);

    const auto fisheyePreviewController(new FisheyePreviewController(this));

    connect(ui->fisheyeWidget,
        &FisheyeSettingsWidget::dataChanged,
        this,
        [this, fisheyePreviewController]
        {
            if (!m_updating)
                fisheyePreviewController->preview(m_resource, ui->fisheyeWidget->parameters());
        });
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
    const auto resourcePtr = resource->toResourcePtr();

    if (resourcePtr->hasFlags(Qn::still_image))
        m_imageProvider.reset(new BasicImageProvider(QImage(resourcePtr->getUrl()), this));
    else
        m_imageProvider.reset(new FfmpegImageProvider(resourcePtr, QSize(), this));

    m_imageProvider->loadAsync();

    ui->fisheyeWidget->updateFromParams(resource->getDewarpingParams(),
        m_imageProvider.data());
}

void MediaFileSettingsDialog::submitToResource(const QnMediaResourcePtr& resource)
{
    resource->setDewarpingParams(ui->fisheyeWidget->parameters());
}

} // namespace nx::vms::client::desktop
