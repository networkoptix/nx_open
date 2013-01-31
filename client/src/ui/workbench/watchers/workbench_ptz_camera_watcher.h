#ifndef QN_WORKBENCH_PTZ_CAMERA_WATCHER_H
#define QN_WORKBENCH_PTZ_CAMERA_WATCHER_H

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>

#include <ui/workbench/workbench_context_aware.h>

class QnWorkbenchPtzCameraWatcher: public QObject, public QnWorkbenchContextAware {
    Q_OBJECT;
public:
    QnWorkbenchPtzCameraWatcher(QObject *parent = NULL);
    virtual ~QnWorkbenchPtzCameraWatcher();

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
    void at_resource_cameraCapabilitiesChanged(const QnSecurityCamResourcePtr &resource);

private:
    QSet<QnVirtualCameraResourcePtr> m_ptzCameras;
};




#endif // QN_WORKBENCH_PTZ_CAMERA_WATCHER_H
