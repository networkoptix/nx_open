#ifndef QN_WORKBENCH_PTZ_CAMERAS_WATCHER_H
#define QN_WORKBENCH_PTZ_CAMERAS_WATCHER_H

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>

#include <ui/workbench/workbench_context_aware.h>

class QnWorkbenchPtzCamerasWatcher: public QObject, public QnWorkbenchContextAware {
    Q_OBJECT;
public:
    QnWorkbenchPtzCamerasWatcher(QObject *parent = NULL);
    virtual ~QnWorkbenchPtzCamerasWatcher();

    QList<QnVirtualCameraResourcePtr> ptzCameras() const;

signals:
    void ptzCameraAdded(const QnVirtualCameraResourcePtr &camera);
    void ptzCameraRemoved(const QnVirtualCameraResourcePtr &camera);

private:
    void addPtzCamera(const QnVirtualCameraResourcePtr &camera);
    void removePtzCamera(const QnVirtualCameraResourcePtr &camera);

private slots:
    void at_resourcePool_resourceAdded(const QnResourcePtr &resource);
    void at_resourcePool_resourceRemoved(const QnResourcePtr &resource);
    void at_cameraResource_cameraCapabilitiesChanged(const QnVirtualCameraResourcePtr &camera);
    void at_cameraResource_cameraCapabilitiesChanged();

private:
    QSet<QnVirtualCameraResourcePtr> m_ptzCameras;
};




#endif // QN_WORKBENCH_PTZ_CAMERAS_WATCHER_H
