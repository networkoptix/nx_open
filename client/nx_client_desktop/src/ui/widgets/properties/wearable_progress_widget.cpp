#include "wearable_progress_widget.h"
#include "ui_wearable_progress_widget.h"

#include <client/client_module.h>

#include <core/resource/camera_resource.h>

#include <nx/client/desktop/ui/actions/action_manager.h>
#include <nx/client/desktop/ui/actions/actions.h>
#include <nx/client/desktop/utils/wearable_manager.h>

using namespace nx::client::desktop;

QnWearableProgressWidget::QnWearableProgressWidget(QWidget* parent):
    QWidget(parent),
    QnWorkbenchContextAware(parent, true),
    ui(new Ui::WearableProgressWidget)
{
    ui->setupUi(this);

    connect(qnClientModule->wearableManager(), &WearableManager::progress, this,
        [this](qreal progress)
        {
            ui->uploadProgressBar->setValue(progress * 100);
        }
    );
}

QnWearableProgressWidget::~QnWearableProgressWidget()
{

}

void QnWearableProgressWidget::setCamera(const QnVirtualCameraResourcePtr &camera)
{
    m_camera = camera;
}

QnVirtualCameraResourcePtr QnWearableProgressWidget::camera() const
{
    return m_camera;
}
