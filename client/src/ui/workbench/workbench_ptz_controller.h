#ifndef QN_WORKBENCH_PTZ_CONTROLLER_H
#define QN_WORKBENCH_PTZ_CONTROLLER_H

#include <QtCore/QObject>
#include <QtCore/QHash>
#include <QtGui/QVector3D>

#include <core/resource/resource_fwd.h>

#include "workbench_context_aware.h"

class QnWorkbenchPtzController: public QObject, public QnWorkbenchContextAware {
    Q_OBJECT;
public:
    QnWorkbenchPtzController(QObject *parent = NULL);
    virtual ~QnWorkbenchPtzController();

    /**
     * \param camera                    Camera to get PTZ position for.
     * \returns                         Current PTZ position for the given camera,
     *                                  or NaN if it is not available.
     *                                  Use <tt>qIsNaN</tt> to check for NaN.
     */
    QVector3D position(const QnVirtualCameraResourcePtr &camera);

    /**
     * \param camera                    Camera to set PTZ position for.
     * \param position                  New PTZ position for the given camera.
     */
    void setPosition(const QnVirtualCameraResourcePtr &camera, const QVector3D &position);

    /**
     * \param camera                    Camera to get current PTZ continuous movement speed for.
     * \returns                         Current PTZ continuous movement speed for the given
     *                                  camera, or NaN if it is not available.
     *                                  Use <tt>qIsNaN</tt> to check for NaN.
     */
    QVector3D movement(const QnVirtualCameraResourcePtr &camera);

    /**
     * \param camera                    Camera to set current PTZ continuous movement speed for.
     * \param movement                  New PTZ continuous movement speed for the given camera.
     */
    void setMovement(const QnVirtualCameraResourcePtr &camera, const QVector3D &movement);

private slots:
    void at_ptzCameraWatcher_ptzCameraAdded(const QnVirtualCameraResourcePtr &camera);
    void at_ptzCameraWatcher_ptzCameraRemoved(const QnVirtualCameraResourcePtr &camera);

private:
    struct PtzData {
        QVector3D position;
        QVector3D movement;
    };

    QHash<QnVirtualCameraResourcePtr, PtzData> m_dataByCamera;
    QHash<int, QnVirtualCameraResourcePtr> m_cameraByHandle;
};


#endif // QN_WORKBENCH_PTZ_CONTROLLER_H
