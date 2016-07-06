
#include "current_user_available_cameras_watcher.h"

#include <ui/workbench/workbench_context.h>
#include <watchers/available_cameras_watcher.h>

QnCurrentUserAvailableCamerasWatcher::QnCurrentUserAvailableCamerasWatcher(QObject *parent)
    : base_type(parent)
    , QnWorkbenchContextAware(parent)
    , m_watcher(new QnAvailableCamerasWatcher(this))
{
    const auto updateOnCamerasChange = [this](const QnResourcePtr & /* cameraResource */)
    {
        emit availableCamerasChanged();
    };

    connect(m_watcher, &QnAvailableCamerasWatcher::cameraAdded, this, updateOnCamerasChange);
    connect(m_watcher, &QnAvailableCamerasWatcher::cameraRemoved, this, updateOnCamerasChange);

    connect(context(), &QnWorkbenchContext::userChanged, this
        , [this](const QnUserResourcePtr &userResource)
    {
        m_watcher->setUser(userResource);
        emit userChanged();
    });
}

QnCurrentUserAvailableCamerasWatcher::~QnCurrentUserAvailableCamerasWatcher()
{
}

QnVirtualCameraResourceList QnCurrentUserAvailableCamerasWatcher::availableCameras() const
{
    return m_watcher->availableCameras();
}
